# media/

Welcome to Chromium Media! This directory primarily contains a collection of
components related to media capture and playback.  Feel free to reach out to the
media-dev@chromium.org mailing list with questions.

As a top level component this may be depended on by almost every other Chromium
component except base/. Certain components may not work properly in sandboxed
processes.



# Directory Breakdown

* audio/ - Code for audio input and output. Includes platform specific output
and input implementations. Due to use of platform APIs, can not normally be used
from within a sandboxed process.

* base/ - Contains miscellaneous enums, utility classes, and shuttling
primitives used throughout `media/` and beyond; i.e. `AudioBus`, `AudioCodec`, and
`VideoFrame` just to name a few. Can be used in any process.

* blink/ - Code for interfacing with the Blink rendering engine for `MediaStreams`
as well as `<video>` and `<audio>` playback. Used only in the same process as Blink;
typically the render process.

* capture/ - Contains content (as in the content layer) capturing and platform
specific video capture implementations.

* cast/ - Contains the tab casting implementation; not to be confused with the
Chromecast code which lives in the top-level cast/ directory.

* cdm/ - Contains code related to the Content Decryption Module (CDM) used for
playback of content via Encrypted Media Extensions (EME).

* device_monitors/ - Contains code for monitoring device changes; e.g. webcam
and microphone plugin and unplug events.

* ffmpeg/ - Contains binding code and helper methods necessary to use the ffmpeg
library located in //third_party/ffmpeg.

* filters/ - Contains data sources, decoders, demuxers, parsers, and rendering
algorithms used for media playback.

* formats/ - Contains parsers used by Media Source Extensions (MSE).

* gpu/ - Contains the platform hardware encoder and decoder implementations.

* midi/ - Contains the WebMIDI API implementation.

* mojo/ - Contains mojo services for media. Typically used for providing out of
process media functionality to a sandboxed process.

* muxers/ - Code for muxing content for the Media Recorder API.

* remoting/ - Code for transmitting muxed packets to a remote endpoint for
playback.

* renderers/ - Code for rendering audio and video to an output sink.

* test/ - Code and data for testing the media playback pipeline.

* video/ - Abstract hardware video decoder interfaces and tooling.



# Capture

TODO(miu, chfemer): Fill in this section.



# mojo

See [media/mojo documentation](/media/mojo).



# MIDI

TODO(toyoshim): Fill in this section.



# Playback

Media playback encompasses a large swatch of technologies, so by necessity this
will provide only a brief outline. Inside this directory you'll find components
for media demuxing, software and hardware video decode, audio output, as well as
audio and video rendering.

Specifically under the playback heading, media/ contains the implementations of
components required for HTML media elements and extensions:

* [HTML5 Audio & Video](https://www.w3.org/html/wg/spec/video.html)
* [Media Source Extensions](https://www.w3.org/TR/media-source/)
* [Encrypted Media Extensions](https://www.w3.org/TR/encrypted-media/)

The following diagram provides a simplified overview of the media playback
pipeline.

![Media Pipeline Overview](/docs/media/media_pipeline_overview.png)

As a case study we'll consider the playback of a video through the `<video>` tag.

`<video>` (and `<audio>`) starts in `blink::HTMLMediaElement` in
third_party/blink/ and reaches third_party/blink/public/platform/media/ in
`media::WebMediaPlayerImpl` after a brief hop through `content::MediaFactory`.
Each `blink::HTMLMediaElement` owns a `media::WebMediaPlayerImpl` for handling
things like play, pause, seeks, and volume changes (among other things).

`media::WebMediaPlayerImpl` handles or delegates media loading over the network
as well as demuxer and pipeline initialization. `media::WebMediaPlayerImpl`
owns a `media::PipelineController` which manages the coordination of a
`media::DataSource`, `media::Demuxer`, and `media::Renderer` during playback.

During a normal playback, the `media::Demuxer` owned by WebMediaPlayerImpl may
be either `media::FFmpegDemuxer` or `media::ChunkDemuxer`. The ffmpeg variant
is used for standard src= playback where WebMediaPlayerImpl is responsible for
loading bytes over the network. `media::ChunkDemuxer` is used with Media Source
Extensions (MSE), where JavaScript code provides the muxed bytes.

The media::Renderer is typically `media::RendererImpl` which owns and
coordinates `media::AudioRenderer` and `media::VideoRenderer` instances. Each
of these in turn own a set of `media::AudioDecoder` and `media::VideoDecoder`
implementations. Each issues an async read to a `media::DemuxerStream` exposed
by the `media::Demuxer` which is routed to the right decoder by
`media::DecoderStream`. Decoding is again async, so decoded frames are
delivered at some later time to each renderer.

The media/ library contains hardware decoder implementations in media/gpu for
all supported Chromium platforms, as well as software decoding implementations
in media/filters backed by FFmpeg and libvpx. Decoders are attempted in the
order provided via the `media::RendererFactory`; the first one which reports
success will be used for playback (typically the hardware decoder for video).

Each renderer manages timing and rendering of audio and video via the event-
driven `media::AudioRendererSink` and `media::VideoRendererSink` interfaces
respectively. These interfaces both accept a callback that they will issue
periodically when new audio or video frames are required.

On the audio side, again in the normal case, the `media::AudioRendererSink` is
driven via a `base::SyncSocket` and shared memory segment owned by the browser
process. This socket is ticked periodically by a platform level implementation
of `media::AudioOutputStream` within media/audio.

On the video side, the `media::VideoRendererSink` is driven by async callbacks
issued by the compositor to `media::VideoFrameCompositor`. The
`media::VideoRenderer` will talk to the `media::AudioRenderer` through a
`media::TimeSource` for coordinating audio and video sync.

With that we've covered the basic flow of a typical playback. When debugging
issues, it's helpful to review the internal logs at chrome://media-internals.
The internals page contains information about active
`media::WebMediaPlayerImpl`, `media::AudioInputController`,
`media::AudioOutputController`, and `media::AudioOutputStream` instances.



# Logging

Media playback typically involves multiple threads, in many cases even multiple
processes. Media operations are often asynchronous running in a sandbox. These
make attaching a debugger (e.g. GDB) sometimes less efficient than other
mechanisms like logging.

## DVLOG

In media we use DVLOG() a lot. It makes filename-based filtering super easy.
Within one file, not all logs are created equal. To make log filtering
more convenient, use appropriate log levels. Here are some general
recommendations:

* DVLOG(1): Once per playback events or other important events, e.g.
  construction/destruction, initialization, playback start/end, suspend/resume,
  any error conditions.
* DVLOG(2): Recurring events per playback, e.g. seek/reset/flush, config change.
* DVLOG(3): Frequent events, e.g. demuxer read, audio/video buffer decrypt or
  decode, audio/video frame rendering.

## MediaLog

MediaLog will send logs to `about://media-internals`, which is easily accessible
by developers (including web developes), testers and even users to get detailed
information about a playback instance. For guidance on how to use MediaLog, see
`media/base/media_log.h`.

MediaLog messages should be concise and free of implementation details. Error
messages should provide clues as to how to fix them, usually by precisely
describing the circumstances that led to the error. Use properties, rather
than messages, to record metadata and state changes.

## Logging Format

When adding logs, it's often helpful to log the function name, e.g.
```
DVLOG(?) << __func__;
```

When adding logs with values, prefer the following format for consistency and
readability:
```
DVLOG(?) << __func__ << ": param1=" << param1 << ", param2=" << param2;
```
