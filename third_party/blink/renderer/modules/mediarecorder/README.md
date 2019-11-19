# Media Stream Recording API

This folder contains the implementation of the [W3C MediaStream Recording API].
Image Capture was shipped in Chrome 49 (Mar 2016) -- support in other browsers
can be found in the [CanIUse] entry.

Encoding video uses hardware accelerated capabilities where possible: check
[Encode Accelerator Implementation Status] for the current situation.

[W3C MediaStream Recording API]: https://w3c.github.io/mediacapture-record/
[CanIUse]: http://caniuse.com/#feat=mediarecorder
[Encode Accelerator Implementation Status]: https://github.com/yellowdoge/mediacapture-record-implementation-status/blob/master/chromium.md

## API Mechanics

A `MediaRecorder` uses a `MediaStream` as its source of data. The stream may
originate from a camera, microphone, `<canvas>`, `<video>` or `<audio>` tag,
remote `PeerConnection`, web audio `Node` or content capture (such as the
screen, a window or a tab).

### Construction Options

The `MediaRecorder()` constructor accepts an optional [`MediaRecorderOptions`]
dictionary giving hints as to how to carry out the encoding:

- `mimeType` indicates which container and codec to use, e.g.
 `video/webm;codecs="vp9"` or `video/x-matroska;codecs="avc1"` (see the specific
 [isTypeSupported()] test).

  Chrome will select the best encoding format if `mimeType` is left
  unspecified; in particular, it will select a hardware accelerated encoder if
  available. (The actual encoding format can be found in `ondataavailable`
 `Blob`s type).

- Users can vary the target encoding bitrate to accommodate different scenes and
CPU loads via the different bitrate members.

### Recording

Once a `MediaRecorder` is created, recording can begin with `start()`.

This method accepts an optional `timeslice` parameter. Chrome will buffer this
much of the encoded result (in milliseconds). If unspecified Chrome will buffer
as much as possible. A value of 0 will cause as little buffering as possible.

Encoded chunks are received via the `ondataavailable` event, following the
cadence specified by the `timeslice`. If `timeslice` is unspecified, the buffer
can be flushed using `requestData()` or `stop()`.  `event.data` contains the
recorded `Blob`.

[`MediaRecorderOptions`]: https://w3c.github.io/mediacapture-record/#mediarecorderoptions-section
[isTypeSupported()]: https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/web_tests/fast/mediarecorder/MediaRecorder-isTypeSupported.html

## Implementation in Chromium

This API is structured around the [MediaRecorder class], which owns a
[`MediaRecorderHandler`] which in turn owns a number of
`VideoTrackRecorder`/`AudioTrackRecorder`s and a single `WebmMuxer`.
[`VideoTrackRecorder`]s are codec specific and encapsulate the necessary resources
to get the job done.  All this is illustrated in the [diagram] below.


[MediaRecorder class]: https://w3c.github.io/mediacapture-record/#mediarecorder-api
[`MediaRecorder()`]: (https://w3c.github.io/mediacapture-record/#mediarecorder-constructor)
[`MediaRecorderHandler`]: (https://chromium.googlesource.com/chromium/src/+/master/content/renderer/media_recorder/media_recorder_handler.h)
[`VideoTrackRecorder`]: https://chromium.googlesource.com/chromium/src/+/master/content/renderer/media_recorder/video_track_recorder.h
[diagram]: http://ibb.co/mLK4Y5

![MediaRecorder classes](http://preview.ibb.co/j1RjY5/DD_Media_Capabilities_Encoding.png)

## Other topics

### Can `MediaRecorder` record stereo?

Yes it can, but Chrome's implementation of audio streams doesn't support this
format, see [crbug/706013] and [crbug/596182].

[crbug/706013]: https://crbug.com/706013
[crbug/596182]: https://crbug.com/596182

### The produced recording doesn't have duration or is not seekable

This is by design of the webm live format and is tracked in [crbug/642012]. The
alternative is to use a Javascript library to reconstruct the Cues (see the
[discussion] in the Spec), or feed the individual recorded chunks into a
`<video>` via a `SourceBuffer`.

[crbug/642012]: https://crbug.com/642012
[discussion]: https://github.com/w3c/mediacapture-record/issues/119

## Testing

Media Recorder web tests are located in [web_tests/fast/mediarecorder], and
[web_tests/external/mediacapture-record], unittests in [content] and [media]
and [browsertests].

[web_tests/fast/mediarecorder]: https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/web_tests/fast/mediarecorder/
[web_tests/external/mediacapture-record]: https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/web_tests/external/wpt/mediacapture-record/
[content]: https://chromium.googlesource.com/chromium/src/+/master/content/renderer/media_recorder/
[media]: https://chromium.googlesource.com/chromium/src/+/master/media/muxers
[browsertests]: https://chromium.googlesource.com/chromium/src/+/master/content/browser/webrtc/webrtc_media_recorder_browsertest.cc

