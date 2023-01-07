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
  * LaCrOS: `//content/browser/media/capture/desktop/desktop_capturer_lacros.cc`
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

## getUserMedia()

Web API for Web applications to capture camera and microphone input.

* Code
  * `//third_party/blink/renderer/modules/mediastream/`
* Issues
  * `Blink>GetUserMedia`
* Docs
  * To be added later.


## getDisplayMedia()

Web API for Web applications to capture screen contents and system/tab audio.

* Code
  * `//third_party/blink/renderer/modules/mediastream/`
* Issues
  * `Blink>GetDisplayMedia`
* Docs
  * To be added later.

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
