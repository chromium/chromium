# Audio Focus Handling

A MediaSession collects all audio-producing objects in one tab. It is usually
unpleasant when multiple MediaSessions play sound at the same time. Audio focus
handling manages the MediaSessions and mixes them in proper ways. This is part
of the default media session on desktop project.

[TOC]

## Processing model

### Audio focus types

There are "persistent" and "transient" audio focus types.

* Persistent audios are used for long media playback, and they should not mix
  with each other. When they start to play, they should pause all other
  playbacks.
* Transient audios are used for short media playback such as a ping for incoming
  message. When they start to play, they should play on top of other playbacks
  and the other playbacks should duck (have reduced volume).

### `MediaSession`

Audio-producing objects should join `MediaSession` when they want to produce
sound. `MediaSession` has the following states:

* ACTIVE: the `MediaSession` has audio focus and its audio-producing objects can
  play.
* SUSPENDED: the MediaSession does not have audio focus. All audio-producing
  objects are paused and can be resumed when the session gains audio focus.
* INACTIVE: the MediaSession does not have audio focus, and there is no
  audio-producing objects in this `MediaSession`.

Besides, `MediaSession` has a `DUCKING` flag, which means its managed
audio-producing objects has lowered volume. The flag is orthogonal with
`MediaSession` state.

### `AudioFocusManager`

`AudioFocusManager` is a global instance which manages the state of
`MediaSession`s. It is used for platforms (e.g. Android) that do not have a
system audio focus.

When an audio-producing object wants to play audio, it should join `MediaSession`
and tell which kind of audio focus type it requires. `MediaSession` will then
request audio focus from `AudioFocusManager`, and will allow the object to play
sound if successful. `AudioFocusManager` will notify other `MediaSession`s if
their states are changed.

When an audio-producing object stops playing audio, it should be removed from
its `MediaSession`, and `MediaSession` should abandon its audio focus if its
audio-producing objects is empty. `AudioFocusManager` will notify other
`MediaSession`s of state change if necessary.

## The algorithm for handling audio focus

`AudioFocusManager` uses a stack implementation. It keeps track of all
ACTIVE/SUSPENDED `MediaSession`s. When a `MediaSession` requests audio focus, it
will be put at the top of the stack, and will be removed from the stack when it
abandons audio focus.

The algorithm is as follows:

* When a `MediaSession` requests audio focus:

  * Remove it from the audio focus stack if it's already there, and place it at
    the top of audio focus stack, grant focus to the session and let it play.
  * If the session is persistent, suspend all the other sessions on the stack.
  * If the session is transient, we should duck any active persistent audio
    focus entry if present:

    * If the next top entry is transient, do nothing, since if there is any
      persistent session that is active, it is already ducking.
    * If the next top entry is persistent, let the next top entry start ducking,
      since it is the only active persistent session.

* When a `MediaSession` abandons audio focus:

  * If the session is not on the top, just remove it from the stack.
  * If the session is on the top, remove it from the stack.

    * If the stack becomes empty, do nothing.
    * If the next top session is transient, do nothing.
    * If the next top session is persistent, stop ducking it.
