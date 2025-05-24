# ChromeOS FaceGaze

FaceGaze (publicly named "Face control") is a ChromeOS accessibility feature
that allows users to control the cursor with their head and perform various
actions using facial gestures.

## Summary

### User flow

FaceGaze can be enabled either in the accessibility quick settings menu or in
the ChromeOS settings app under the route Accessibility > Cursor and touchpad >
Face control. Once FaceGaze is enabled, the face recognition model and backing
web assembly will be downloaded via DLC (downloadable content). When the
download succeeds, the face model gets initialized and the webcam is turned on.
The user can then move the cursor with their head and perform actions with
facial gestures. When recognized, gestures and their associated actions will be
posted to the FaceGaze bubble UI, which is a floating UI component positioned at
the top of the display.

FaceGaze has several actions that temporarily put FaceGaze into a different
state. Examples include enter/exit scroll mode, start/end long click, pause/
resume FaceGaze, and start/stop Dictation. When scroll mode is active, for
example, head movements will not move the mouse but instead be used to determine
a scroll direction. When FaceGaze is in an alternate state, it will be
communicated via the bubble UI.

Note that if the DLC download fails, FaceGaze will automatically turn off and a
notification will be shown with a failure message.

### Technical overview

FaceGaze is implemented primarily as a Chrome extension in TypeScript. It also
has a few browser-side components (DLC hook and APIs), as well as ash-side
components (bubble UI). The high-level components of the feature are:

1. The Chrome extension, which is where most of the logic lives
2. A hook in the extension to connect to the device's webcam
3. An ML model, called [FaceLandmarker](https://ai.google.dev/edge/mediapipe/solutions/vision/face_landmarker),
which processes video frames and returns results containing the location of all
relevant face points, confidences for facial gestures, and the amount of head
rotation. This is the technology that makes FaceGaze possible.
4. Extension APIs to update the cursor position, send synthetic mouse and key
events, and interact with the FaceGaze bubble in the browser (among other
things)
5. The ash-side implementation for the bubble UI
6. Settings page implementation, where users can configure their cursor
settings and update their gesture-to-action bindings

Once FaceGaze is initialized, here's a high-level flow of how it responds to a
single camera frame:

1. FaceGaze will grab the latest frame from the webcam feed
2. The frame is forwarded to the FaceLandmarker, which returns a raw result with
face points, gesture confidences, and head rotation
3. FaceGaze will further interpret this result and convert facial gestures to
actions (called "macros" in the code) depending on the user's preferences
4. FaceGaze will update the mouse location, perform actions, and update the floating bubble UI
5. The above process is repeated many times per second to give the user a
feeling of responsiveness, e.g. mouse movement responds quickly to head movement

As mentioned above, FaceGaze utilizes a [DLC](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/app-accessibility/facegaze-assets/)
to supply the FaceLandmarker model and the backing web assembly.

### Accessing the webcam feed

FaceGaze utilizes the [webRTC API](https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_API),
specifically the [ImageCapture API](https://developer.mozilla.org/en-US/docs/Web/API/ImageCapture)
to grab video frames and pass them to the FaceLandmarker model.

## Code structure

The majority of FaceGaze code lives in the [facegaze/](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/chromeos/accessibility/accessibility_common/facegaze/) extension directory. Settings code lives in
[chrome/browser/resources/ash/settings/os_a11y_page](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/ash/settings/os_a11y_page/).
Code for the bubble UI lives in [ash/system/accessibility](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/accessibility/).

### FaceGaze extension classes

The `facegaze/` extension directory contains several noteworthy classes:

* `FaceGaze`, which is the main object. It handles setup/teardown, interacts
with APIs like chrome.settingsPrivate, and owns the other essential classes.

* `WebCamFaceLandmarker`, which requests the DLC download, initializes the
FaceLandmarker API, starts the webcam, continually passes frames from the video
stream into the FaceLandmarker while the video stream is active, and returns
results to the main `FaceGaze` object.

* `GestureDetector`, which computes which gestures were detected, filtering
out those with low confidence scores. It also transforms raw gestures into
ones supported by FaceGaze; for example, FaceGaze doesn't support "blink left
eye" and "blink right eye" individually. Instead, it supports a compound
"blink eyes" gesture.

* `GestureHandler`, which does additional processing of FaceLandmarker results
and converts recognized gestures into executable macros.

* `MouseController`, which similarly processes FaceLandmarker results to convert
recognized face points and rotation into a new cursor location. This class also
contains logic to smooth cursor movement so that the user gets natural cursor
movements instead of jumpy cursor movements.

* `ScrollModeController`, which gives users scroll functionality with FaceGaze.

* `BubbleController`, which controls all interaction with the FaceGaze bubble
UI.

### FaceGaze ash-side classes

* `FaceGazeBubbleController` manages the FaceGaze UI from ash and provides an
entry point for updating/changing the UI.

* `FaceGazeBubbleView` is the actual implementation of the FaceGaze UI.

### FaceGaze browser-side classes

* `AccessibilityManager` contains logic for setting up/tearing down the
extension, forwarding requests and results for DLC downloads, and showing
notifications to the user.

* `AccessibilityDlcInstaller` performs the install of the facegaze-assets DLC
and passes the contents through to the extension.

* `DragEventRewriter` is a common class that helps implement drag and drop for
Autoclick and FaceGaze. While the class is active, all mouse movement events
will be rewritten into mouse drag events.

### FaceGaze settings

TODO

### Testing

* See the `facegaze/` extension directory for all extension tests.

* See [facegaze_browsertest.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/accessibility/facegaze_browsertest.cc)
for C++ integration tests. Note that these tests hook into a JavaScript class
called `FaceGazeTestSupport` and allows the C++ tests to execute JavaScript or
wait for information to propagate to the extension side before continuing.
[facegaze_test_utils.cc](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/accessibility/facegaze_test_utils.cc)
contains test support for writing tests.

* See [facegaze.go](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/a11y/facegaze/facegaze.go)
which provides infrastructure for FaceGaze in tast. Also see [idle_perf.go](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/bundles/cros/ui/idle_perf.go),
which runs FaceGaze idly for ten minutes and collects performance metrics across
many different types of physical devices.
