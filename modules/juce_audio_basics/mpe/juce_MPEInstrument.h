/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2015 - ROLI Ltd.

   Permission is granted to use this software under the terms of either:
   a) the GPL v2 (or any later version)
   b) the Affero GPL v3

   Details of these licenses can be found at: www.gnu.org/licenses

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.juce.com for more information.

  ==============================================================================
*/

#ifndef JUCE_MPEINSTRUMENT_H_INCLUDED
#define JUCE_MPEINSTRUMENT_H_INCLUDED


//==============================================================================
/*
    This class represents an instrument handling MPE.

    It has an MPE zone layout and maintans a state of currently
    active (playing) notes and the values of their dimensions of expression.

    You can trigger and modulate notes:
     - by passing MIDI messages with the method processNextMidiEvent;
     - by directly calling the methods noteOn, noteOff etc.

    The class implements the channel and note management logic specified in
    MPE. If you pass it a message, it will know what notes on what
    channels (if any) should be affected by that message.

    The class has a Listener class with the three callbacks MPENoteAdded,
    MPENoteChanged, and MPENoteFinished. Implement such a
    Listener class to react to note changes and trigger some functionality for
    your application that depends on the MPE note state.
    For example, you can use this class to write an MPE visualiser.

    If you want to write a real-time audio synth with MPE functionality,
    you should instead use the classes MPESynthesiserBase, which adds
    the ability to render audio and to manage voices.

    @see MPENote, MPEZoneLayout, MPESynthesiser
*/
class JUCE_API  MPEInstrument
{
public:

    /** Constructor.
        This will construct an MPE instrument with initially no MPE zones.

        In order to process incoming MIDI, call setZoneLayout, define the layout
        via MIDI RPN messages, or set the instrument to omni mode.
    */
    MPEInstrument() noexcept;

    /** Destructor. */
    virtual ~MPEInstrument();

    //==========================================================================
    /** Returns the current zone layout of the instrument.
        This happens by value, to enforce thread-safety and class invariants.

        Note: If the instrument is in Omni mode, the return value of this
        method is unspecified.
    */
    MPEZoneLayout getZoneLayout() const noexcept;

    /** Re-sets the zone layout of the instrument to the one passed in.
        As a side effect, this will discard all currently playing notes,
        and call noteReleased for all of them.

        This will also disable Omni Mode in case it was enabled previously.
    */
    void setZoneLayout (MPEZoneLayout newLayout);

    /** Sets the instrument to Omni Mode.
        As a side effect, this will discard all currently playing notes,
        and call noteReleased for all of them.

        This special zone layout mode is for backwards compatibility with
        non-MPE MIDI devices. In this mode, the instrument will ignore the
        current zone layout. It will instead treat all 16 MIDI channels as note
        channels, with no master channel.

        @param pitchbendRange   The pitchbend range in semitones that should be
                                used while the instrument is in Omni mode. Must
                                be between 0 and 96, otherwise behaviour is undefined.
    */
    void enableOmniMode (int pitchbendRange = 2);

    /** Returns true if the instrument is in Omni mode, false otherwise. */
    bool isOmniModeEnabled() const noexcept;

    //==========================================================================
    /** The MPE note tracking mode. In case there is more than one note playing
        simultaneously on the same MIDI channel, this determines which of these
        notes will be modulated by an incoming MPE message on that channel
        (pressure, pitchbend, or timbre).

        The default is lastNotePlayedOnChannel.
    */
    enum TrackingMode
    {
        lastNotePlayedOnChannel, //! The most recent note on the channel that is still played (key down and/or sustained)
        lowestNoteOnChannel,     //! The lowest note (by initialNote) on the channel with the note key still down
        highestNoteOnChannel,    //! The highest note (by initialNote) on the channel with the note key still down
        allNotesOnChannel        //! All notes on the channel (key down and/or sustained)
    };

    /** Set the MPE tracking mode for the pressure dimension. */
    void setPressureTrackingMode (TrackingMode modeToUse);

    /** Set the MPE tracking mode for the pitchbend dimension. */
    void setPitchbendTrackingMode (TrackingMode modeToUse);

    /** Set the MPE tracking mode for the timbre dimension. */
    void setTimbreTrackingMode (TrackingMode modeToUse);

    //==========================================================================
    /** Process a MIDI message and trigger the appropriate method calls
        (noteOn, noteOff etc.)

        You can override this method if you need some special MIDI message
        treatment on top of the standard MPE logic implemented here.
    */
    virtual void processNextMidiEvent (const MidiMessage& message);

    //==========================================================================
    /** Request a note-on on the given channel, with the given initial note
        number and velocity.
        If the message arrives on a valid note channel, this will create a
        new MPENote and call the noteAdded callback.
    */
    virtual void noteOn (int midiChannel, int midiNoteNumber, MPEValue midiNoteOnVelocity);

    /** Request a note-off. If there is a matching playing note, this will
        release the note (except if it is sustained by a sustain or sostenuto
        pedal) and call the noteReleased callback.
    */
    virtual void noteOff (int midiChannel, int midiNoteNumber, MPEValue midiNoteOffVelocity);

    /** Request a pitchbend on the given channel with the given value (in units
        of MIDI pitchwheel position).
        Internally, this will determine whether the pitchwheel move is a
        per-note pitchbend or a master pitchbend (depending on midiChannel),
        take the correct per-note or master pitchbend range of the affected MPE
        zone, and apply the resulting pitchbend to the affected note(s) (if any).
    */
    virtual void pitchbend (int midiChannel, MPEValue pitchbend);

    /** Request a pressure change on the given channel with the given value.
        This will modify the pressure dimension of the note currently held down
        on this channel (if any). If the channel is a zone master channel,
        the pressure change will be broadcast to all notes in this zone.
    */
    virtual void pressure (int midiChannel, MPEValue value);

    /** Request a third dimension (timbre) change on the given channel with the
        given value.
        This will modify the timbre dimension of the note currently held down
        on this channel (if any). If the channel is a zone master channel,
        the timbre change will be broadcast to all notes in this zone.
    */
    virtual void timbre (int midiChannel, MPEValue value);

    /** Request a sustain pedal press or release. If midiChannel is a zone's
        master channel, this will act on all notes in that zone; otherwise,
        nothing will happen.
    */
    virtual void sustainPedal (int midiChannel, bool isDown);

    /** Request a sostenuto pedal press or release. If midiChannel is a zone's
        master channel, this will act on all notes in that zone; otherwise,
        nothing will happen.
    */
    virtual void sostenutoPedal (int midiChannel, bool isDown);

    /** Discard all currently playing notes.
        This will also call the noteReleased listener callback for all of them.
    */
    void releaseAllNotes();

    //==========================================================================
    /** Returns the number of MPE notes currently played by the
        instrument.
    */
    int getNumPlayingNotes() const noexcept;

    /** Returns the note at the given index. If there is no such note, returns
        an invalid MPENote. The notes are sorted such that the most recently
        added note is the last element.
    */
    MPENote getNote (int index) const noexcept;

    /** Returns the note currently playing on the given midiChannel with the
        specified initial MIDI note number, if there is such a note.
        Otherwise, this returns an invalid MPENote
        (check with note.isValid() before use!)
    */
    MPENote getNote (int midiChannel, int midiNoteNumber) const noexcept;

    /** Returns the most recent note that is playing on the given midiChannel
        (this will be the note which has received the most recent note-on without
        a corresponding note-off), if there is such a note.
        Otherwise, this returns an invalid MPENote
        (check with note.isValid() before use!)
    */
    MPENote getMostRecentNote (int midiChannel) const noexcept;

    /** Returns the most recent note that is not the note passed in.
        If there is no such note, this returns an invalid MPENote
        (check with note.isValid() before use!)
        This helper method might be useful for some custom voice handling algorithms.
    */
    MPENote getMostRecentNoteOtherThan (MPENote otherThanThisNote) const noexcept;

    //==========================================================================
    /** Derive from this class to be informed about any changes in the expressive
        MIDI notes played by this instrument.

        Note: This listener type receives its callbacks immediately, and not
        via the message thread (so you might be for example in the MIDI thread).
        Therefore you should never do heavy work such as graphics rendering etc.
        inside those callbacks.
    */
    class Listener
    {
    public:
        /** Constructor. */
        Listener();

        /** Destructor. */
        virtual ~Listener();

        /** Implement this callback to be informed whenever a new expressive
            MIDI note is triggered.
        */
        virtual void noteAdded (MPENote newNote) = 0;

        /** Implement this callback to be informed whenever a currently
            playing MPE note's pressure value changes.
        */
        virtual void notePressureChanged (MPENote changedNote) = 0;

        /** Implement this callback to be informed whenever a currently
            playing MPE note's pitchbend value changes.
            Note: This can happen if the note itself is bent, if there is a
            master channel pitchbend event, or if both occur simultaneously.
            Call MPENote::getFrequencyInHertz to get the effective note frequency.
        */
        virtual void notePitchbendChanged (MPENote changedNote) = 0;

        /** Implement this callback to be informed whenever a currently
            playing MPE note's timbre value changes.
        */
        virtual void noteTimbreChanged (MPENote changedNote) = 0;

        /** Implement this callback to be informed whether a currently playing
            MPE note's key state (whether the key is down and/or the note is
            sustained) has changed.
            Note: if the key state changes to MPENote::off, noteReleased is
            called instead.
        */
        virtual void noteKeyStateChanged (MPENote changedNote) = 0;

        /** Implement this callback to be informed whenever an MPE note
            is released (either by a note-off message, or by a sustain/sostenuto
            pedal release for a note that already received a note-off),
            and should therefore stop playing.
        */
        virtual void noteReleased (MPENote finishedNote) = 0;
    };

    //==========================================================================
    /** Adds a listener. */
    void addListener (Listener* const listenerToAdd) noexcept;

    /** Removes a listener. */
    void removeListener (Listener* const listenerToRemove) noexcept;

protected:
    //==========================================================================
    /** This method defines what initial pitchbend value should be used for newly
        triggered notes. The default is to use the last pitchbend value
        that has been received on the same MIDI channel (or no pitchbend
        if no pitchbend messages have been received so far).
        Override this method if you need different behaviour.
    */
    virtual MPEValue getInitialPitchbendForNoteOn (int midiChannel,
                                                   int midiNoteNumber,
                                                   MPEValue midiNoteOnVelocity) const;

    /** This method defines what initial pressure value should be used for newly
        triggered notes. The default is to re-use the note-on velocity value.
        Override this method if you need different behaviour.
    */
    virtual MPEValue getInitialPressureForNoteOn (int midiChannel,
                                                  int midiNoteNumber,
                                                  MPEValue midiNoteOnVelocity) const;

    /** This method defines what initial timbre value should be used for newly
        triggered notes. The default is to use the last timbre value that has
        that has been received on the same MIDI channel (or a neutral centred value
        if no pitchbend messages have been received so far).
        Override this method if you need different behaviour.
    */
    virtual MPEValue getInitialTimbreForNoteOn (int midiChannel,
                                                int midiNoteNumber,
                                                MPEValue midiNoteOnVelocity) const;

private:
    //==========================================================================
    CriticalSection lock;
    Array<MPENote> notes;
    MPEZoneLayout zoneLayout;
    ListenerList<Listener> listeners;

    uint8 lastPressureLowerBitReceivedOnChannel[16];
    uint8 lastTimbreLowerBitReceivedOnChannel[16];
    bool isNoteChannelSustained[16];

    struct OmniMode
    {
        bool isEnabled;
        int pitchbendRange;
    };

    struct MPEDimension
    {
        MPEDimension() noexcept  : trackingMode (lastNotePlayedOnChannel) {}
        TrackingMode trackingMode;
        MPEValue lastValueReceivedOnChannel[16];
        MPEValue MPENote::* value;
        MPEValue& getValue (MPENote& note) noexcept   { return note.*(value); }
    };

    OmniMode omniMode;
    MPEDimension pitchbendDimension, pressureDimension, timbreDimension;

    void updateDimension (int midiChannel, MPEDimension&, MPEValue);
    void updateDimensionMaster (MPEZone&, MPEDimension&, MPEValue);
    void updateDimensionForNote (MPENote&, MPEDimension&, MPEValue);
    void callListenersDimensionChanged (MPENote&, MPEDimension&);

    void processMidiNoteOnMessage (const MidiMessage&);
    void processMidiNoteOffMessage (const MidiMessage&);
    void processMidiPitchWheelMessage (const MidiMessage&);
    void processMidiChannelPressureMessage (const MidiMessage&);
    void processMidiControllerMessage (const MidiMessage&);
    void processMidiAllNotesOffMessage (const MidiMessage&);
    void handlePressureMSB (int midiChannel, int value) noexcept;
    void handlePressureLSB (int midiChannel, int value) noexcept;
    void handleTimbreMSB (int midiChannel, int value) noexcept;
    void handleTimbreLSB (int midiChannel, int value) noexcept;
    void handleSustainOrSostenuto (int midiChannel, bool isDown, bool isSostenuto);

    bool isNoteChannel (int midiChannel) const noexcept;
    bool isMasterChannel (int midiChannel) const noexcept;
    MPENote* getNotePtr (int midiChannel, int midiNoteNumber) const noexcept;
    MPENote* getNotePtr (int midiChannel, TrackingMode) const noexcept;
    MPENote* getLastNotePlayedPtr (int midiChannel) const noexcept;
    MPENote* getHighestNotePtr (int midiChannel) const noexcept;
    MPENote* getLowestNotePtr (int midiChannel) const noexcept;
    void updateNoteTotalPitchbend (MPENote&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MPEInstrument)
};


#endif // JUCE_MPE_H_INCLUDED