# Media Session

Media Session manages the media session and audio focus for playback across the
entire system.

Clients can use the Audio Focus API to request audio focus and observe changes
through AudioFocusObserver. The Media Session API can be used to control
playback and observe changes through MediaSessionObserver.

For more details about controlling playback see [controlling Media Playback](https://chromium.googlesource.com/chromium/src/+/master/services/media_session/controlling_media_playback.md).

TODO(beccahughes): Write docs about requesting audio focus.

## Media Session IDs

The Media Session service uses base::UnguessableToken for a number of different
reasons, these are:

* Request IDs: These identify a single media session
* Group IDs: These group together a number of media sessions that should share
  focus, this is usually per-app.
* Source IDs: These identity a user/profile that created a media session

Every media session has its own unique request ID and it may also have a group
ID and a source ID.