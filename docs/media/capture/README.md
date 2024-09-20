# Media Capture

In Chrome, "Media capture" refers to the features and user interfaces that
enable the browser to capture pixels and audio from itself or the underlying OS.
It also covers the Web APIs that allow Web applications to obtain captured
media.

This is an index of media capture related documentation elsewhere, organized by
component/feature.  This list is incomplete; additional documentation can be
found in code comments and/or Monorail issue threads in the components listed
below.

# Features

## Webcam capture

* Code
  * `//media/capture/video`
  * `//services/video_capture`
* Issues
  * `Internals>Media>CameraCapture`
* Docs
  * [//services/video_capture/README.md](../../../services/video_capture/README.md)


## Microphone capture

* Code
  * `//media/audio`
  * `//services/audio`
* Issues
  * `Internals>Media>AudioCapture`
* Docs
  * [//services/audio/README.md](../../../services/audio/README.md)


## System (desktop/window) capture

* Code
  * Windows/Linux: `//third_party/webrtc/modules/desktop_capture`
  * Mac: `//content/browser/media/capture/desktop_capture_device_mac.cc`
* Issues
  * `Internals>Media>ScreenCapture`
* Docs
  * To be added later.


## System audio capture

* Code
  * Windows: `//media/audio/win/audio_low_latency_input_win.cc`
  * ChromeOS: `//media/audio/cras/cras_input.cc`
* Issues
  * `Internals>Media>Audio`
* Docs
  * To be added later.


## Tab audio capture

* Code
  * `//services/audio/loopback_stream.cc`
* Issues
  * `Internals>Media>Audio`
* Docs
  * To be added later.


## Viz compositor surface capture

Viz supports capture of Chrome tabs and windows on all platforms, and capture
of the desktop on ChromeOS.

Familiarity with [Viz](../../../components/viz/README.md), the Chrome compositor,
is a prerequisite to understanding how surface capture works.

* Code
  * `//content/browser/media/capture/web_contents_video_capture_device.cc`
  * `//components/viz/service/frame_sinks/video_capture`
  * `//components/viz/common/frame_sinks/copy_output_request.cc`
* Issues
  * `Internals>Media>SurfaceCapture`
* Docs
  * [//components/viz/common/frame_sinks/README.md](../../../components/viz/common/frame_sinks/README.md)


# User interfaces

## Screen capture target chooser

Allows the user to select a tab/window/desktop in response to a getDisplayMedia
request.

* Code
  * `//chrome/browser/media/webrtc`
  * `//chrome/browser/ui/views/desktop_capture`
* Issues
  * [`UI>Browser>MediaCapture`](https://bugs.chromium.org/p/chromium/issues/list?q=component%3AUI%3EBrowser%3EMediaCapture&can=2)
* Docs
  * [Enterprise policies to restrict available sources in chooser](https://docs.google.com/document/d/e/2PACX-1vQi8P2f493UgVNCJzcdxLUqqSlIdixlybO0mEPAvqnea_8l5bcUWSSZCi4M4EzSTCrFGQodmDX4LZ_u/pub)


## Conditional Focus

Not a UI surface, but an API that allows a web application using
`getDisplayMedia()` to control whether or not the captured tab/window is focused
(activated) or not when tab/window capture begins.

* Code
  * [`//chrome/browser/media/webrtc/media_stream_focus_delegate.cc`](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/media/webrtc/media_stream_focus_delegate.cc)
* Issues
  * [`UI>Browser>MediaCapture`](https://bugs.chromium.org/p/chromium/issues/list?q=component%3AUI%3EBrowser%3EMediaCapture&can=2)
* Design doc: [Conditional Focus](https://docs.google.com/document/d/e/2PACX-1vQIlFPntWbJaA9prNfxt5SXoCJCiBONPjj9VJVuOLqrYoHOArsvQahO6WhjP8DflF1YL6FXIt524NFA/pub)


## Screen capture indicator

Widget that tells the user when the desktop is being captured.

* Code
  * `//chrome/browser/ui/screen_capture_notification_ui.cc`
  * `//chrome/browser/ui/views/screen_capture_notification_ui_views.cc`
  * ChromeOS: `//chrome/browser/chromeos/ui/screen_capture_notification_ui_chromeos.cc`
* Issues
  * [`UI>Browser>MediaCapture`](https://bugs.chromium.org/p/chromium/issues/list?q=component%3AUI%3EBrowser%3EMediaCapture&can=2)
* Docs
  * To be added later.


## Tab capture indicator/switcher

Shows the user what tab is being captured and allows them to stop or switch tabs.

* Code
  * `//chrome/browser/media/webrtc/media_stream_capture_indicator.cc`
  * `//chrome/browser/ui/tab_sharing`
  * `//chrome/browser/ui/views/tab_sharing.cc`
* Issues
  * [`UI>Browser>MediaCapture`](https://bugs.chromium.org/p/chromium/issues/list?q=component%3AUI%3EBrowser%3EMediaCapture&can=2)
* Docs
  * To be added later.


## Capture device chooser

Allows the user to switch the camera/microphone used for capture via an icon in the omnibox.

* Code
  * `//chrome/browser/ui/content_settings/content_settings_bubble_model.cc`
* Issues
  * [`UI>Browser>MediaCapture`](https://bugs.chromium.org/p/chromium/issues/list?q=component%3AUI%3EBrowser%3EMediaCapture&can=2)
* Docs
  * To be added later.

# APIs

## [getUserMedia()](https://rawgit.com/w3c/mediacapture-main/master/getusermedia.html)

Web API for Web applications to capture camera and microphone input.

* Code
  * `//third_party/blink/renderer/modules/mediastream/`
  * `//third_party/blink/renderer/platform/webrtc/`
* Issues
  * [`Blink>GetUserMedia`](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ABlink%3EGetUserMedia&can=2)
* Docs
  * To be added later.

## [getDisplayMedia()](https://w3c.github.io/mediacapture-screen-share/)

Web API for Web applications to capture screen contents and system/tab audio.

* Code
  * `//third_party/blink/renderer/modules/mediastream/`
* Issues
  * [`Blink>GetDisplayMedia`](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ABlink%3EGetDisplayMedia&can=2)
* Docs
  * To be added later.

# Logs

Usage of the APIs above involves several media layers in Chrome and can contain
both audio and video streams. Logs from the most essential classes are
centralized and all pass through:
* [`content/browser/renderer_host/media/media_stream_manager.cc`](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/renderer_host/media/media_stream_manager.cc), and
* [`third_party/blink/renderer/platform/webrtc/webrtc_logging.cc`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/platform/webrtc/webrtc_logging.cc)

It is possible to view these logs by adding special log filters (using a local
Chromium build in this example):
```
./out/Default/chrome --enable-logging --vmodule=*/content/browser/renderer_host/media/*=1,*/third_party/blink/renderer/platform/webrtc/*=1
```

Each log line is prepended with a tag corresponding to an abbreviation of the
class which generated the log. E.g. `UMP::` for `UserMediaProcessor`, `PLAS::`
for `ProcessedLocalAudioSource`, `AIC::` for `AudioInputController`, `AMB::` for
`AudioManagerBase` etc. The tags enable post processing of the captured logs to
analyze how a web client uses specified classes or modules in Chrome.

The example below is from a [WebRTC demo client](https://webrtc.github.io/samples/src/content/getusermedia/audio/) which uses `getUserMedia` to captures audio and then render it locally:

```
less ./out/Default/chrome_debug.log | grep 'UMP'

[11460:25180:0817/133550.056:VERBOSE1:webrtc_logging.cc(35)] UMP::ProcessRequest({request_id=0}, {audio=1}, {video=0})
[11460:25180:0817/133550.056:VERBOSE1:webrtc_logging.cc(35)] UMP::SetupAudioInput({request_id=0}, {constraints=})
[11460:25180:0817/133550.056:VERBOSE1:webrtc_logging.cc(35)] UMP::SetupAudioInput({request_id=0}) => (Requesting device capabilities)
[11460:25180:0817/133550.843:VERBOSE1:webrtc_logging.cc(35)] UMP::SelectAudioSettings({request_id=0})
[11460:25180:0817/133550.853:VERBOSE1:webrtc_logging.cc(35)] UMP::GenerateStreamForCurrentRequestInfo({request_id=0}, {audio.device_id=default}, {video.device_id=})
[11460:25180:0817/133550.911:VERBOSE1:webrtc_logging.cc(35)] UMP::OnStreamsGenerated({request_id=0}, {label=b24a1017-a129-4465-b1fa-f4d9c553d956}, {device=[id: default, name: Default - Microphone (Realtek(R) Audio)]})
[11460:25180:0817/133550.911:VERBOSE1:webrtc_logging.cc(35)] UMP::StartTracks({request_id=0}, {label=b24a1017-a129-4465-b1fa-f4d9c553d956})
[11460:25180:0817/133550.913:VERBOSE1:webrtc_logging.cc(35)] UMP::CreateAudioTrack({render_to_associated_sink=0})
[11460:25180:0817/133550.913:VERBOSE1:webrtc_logging.cc(35)] UMP::InitializeAudioSourceObject({session_id=4FF8214D8180F02B8A9206564C59C03D})
[11460:25180:0817/133550.914:VERBOSE1:webrtc_logging.cc(35)] UMP::CreateAudioSource => (audiprocessing is required)
[11460:25180:0817/133550.922:VERBOSE1:webrtc_logging.cc(35)] UMP::StartAudioTrack({track=[id: 9803bb62-5cdc-4279-b58e-fd1193d7ecd8, enabled: 1]}, {is_pending=1})
[11460:25180:0817/133550.922:VERBOSE1:webrtc_logging.cc(35)] UMP::StartAudioTrack(source: {session_id=4FF8214D8180F02B8A9206564C59C03D}, {is_local_source=1}, {device=[id: default, group_id: def1842e91c7bc3d9c2be5618262afa5413975edb13d4a33c266224daed0a478, name: Default - Microphone (Realtek(R) Audio)]})
[11460:25180:0817/133551.330:VERBOSE1:webrtc_logging.cc(35)] UMP::OnTrackStarted({session_id=4FF8214D8180F02B8A9206564C59C03D}, {result=OK})
[11460:25180:0817/133551.330:VERBOSE1:webrtc_logging.cc(35)] UMP::UMP::OnCreateNativeTracksCompleted({request_id=0}, {label=b24a1017-a129-4465-b1fa-f4d9c553d956})
[11460:25180:0817/133551.330:VERBOSE1:webrtc_logging.cc(35)] UMP::GetUserMediaRequestSucceeded({request_id=0})
[11460:25180:0817/133551.331:VERBOSE1:webrtc_logging.cc(35)] UMP::DelayedGetUserMediaRequestSucceeded({request_id=0}, {result=OK})
[11460:25180:0817/133553.114:VERBOSE1:webrtc_logging.cc(35)] UMP::OnDeviceStopped({session_id=4FF8214D8180F02B8A9206564C59C03D}, {device_id=default})
[11460:25180:0817/133553.115:VERBOSE1:webrtc_logging.cc(35)] UMP::StopLocalSource({session_id=4FF8214D8180F02B8A9206564C59C03D})
[11460:25180:0817/133553.123:VERBOSE1:webrtc_logging.cc(35)] UMP::RemoveLocalSource({id=default}, {name=Default - Microphone (Realtek(R) Audio)}, {group_id=def1842e91c7bc3d9c2be5618262afa5413975edb13d4a33c266224daed0a478})
```

# Additional features

Not all media capture features are listed above; here are some additional
capture features in Chrome.  For more information about these, contact
developers at the address below.

* [Capture Handle API](https://w3c.github.io/mediacapture-handle/identity/)
* Tab casting to Chromecast
* Screen casting from the ChromeOS system tray
* Presentation API 1-UA mode
* MediaStream capture from HTML elements (`<video>`, `<canvas>`)
* Extension APIs for capture


# Contact information

Questions about media capture features, APIs, or user interfaces can be sent to
[media-capture-dev@chromium.org](mailto:media-capture-dev@chromium.org).
