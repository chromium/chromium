# QR Scanner

The QR Scanner provides a way of scanning QR codes and bar codes directly from
Chrome. It is developed behind the `EnableQRCodeReader` experimental flag.

[TOC]

## Usage

1.  Create a delegate implementing the `QRScannerViewControllerDelegate`
    protocol.
2.  Initialize `QRScannerViewController` with this delegate.
3.  Present the view controller returned by `viewControllerToPresent`.

## Behavior

The QR Scanner is presented as a full-screen view controller displaying a video
preview, a control for the camera's torch, and a control for closing the QR
scanner.

### Presentation

The QR Scanner is presented using a custom transition animation which makes it
appear to be originally positioned under the presenting view controller.

### Scanning

*   Codes are only recognized inside the viewport.
*   A flash animation is played when a code is recognized.
*   If VoiceOver is enabled, an announcement is played instead of the animation.
*   Scanning a QR code or any other code type which can contain alphanumeric
    strings places the scanned result in the Omnibox, and the user has to press
    the "Go" button on the keyboard to load the result. Non-URL strings will be
    loaded in search.
*   Scanning a bar code type which can only contain numbers will load search
    results containing the bar code immediately without waiting for user
    confirmation.

### Torch

*   Torch is switched off every time the QR scanner is opened, closed, or the
    camera session is interrupted.
*   The torch button always reflects the current torch state.
*   The torch button is in a disabled state if the camera does not have torch,
    the torch is unavailable, or the camera is not yet loaded.

### Errors

*   A dialog is displayed if the camera is unavailable, camera permissions are
    not granted, the camera is in use by another application, or the application
    is in Split View on iPad.
*   Pressing the "Cancel" button of any error dialog dismisses the QR Scanner
    view controller.
*   If the camera becomes available when a dialog is presented, the dialog is
    automatically dismissed.

## Entry points

The QR scanner can be accessed from the 3D Touch application shortcuts on
supported devices. The `SpotlightActions` experiment allows the QR scanner to be
accessed from Spotlight search. More info about Spotlight actions can be found
at go/chrome-ios-spotlight and crbug.com/608733. Planned and rejected entry
points are described in the design doc at go/chrome-ios-qr-code.

Tests for QR Scanner are a part of `ios_chrome_ui_egtests`.

## Controller architecture

*   **QRScannerViewController** is the entry point for the feature. It connects
    the `CameraController` and the `QRScannerView` and is responsible for
    displaying alerts from `QRScannerAlerts`.
*   **CameraController** manages the `AVCaptureSession` for the camera. It is
    responsible for loading the camera, listening for camera notifications,
    receiving the scanned result and informing the `QRScannerViewController`
    about changes to the state of the camera or the torch.

Operations performed by `CameraController` are done on a separate dispatch
queue, as recommended by the [documentation][avcapturesession] for
`AVCaptureSession`.

[avcapturesession]: https://developer.apple.com/reference/avfoundation/avcapturesession

### Initialization

`QRScannerViewController` owns an instance of `CameraController` and
`QRScannerView` and is their delegate.

1.  The `viewControllerToPresent` method checks if camera permission is
    granted by calling `checkPermissionsAndLoadCamera` of the
    `CameraController`.
2.  If the camera permission is denied, an error dialog will be returned from
    `viewControllerToPresent`. If the user has not previously granted camera
    permission to the application, the `QRScannerViewController` instance will
    be returned, and an error will be displayed by the QR scanner if the user
    denies the permission in the system dialog. The error dialog prompts the
    user to change this setting and includes a link to the Settings app, if
    available.
3.  If the camera permission is granted, the `QRScannerViewController` will be
    returned as the view controller to present, and the `CameraController` will
    start loading the camera on a separate dispatch queue.

#### Camera

Camera initialization is handled by the `loadCaptureSession` method of the
`CameraController`.

1.  Camera state is set to `CAMERA_UNAVAILABLE` and an error dialog is
    displayed if:
    *   The back camera of the device is not found,
    *   There was an error initializing the camera input,
    *   The video input cannot be attached to the `AVCaptureSession`,
    *   The metadata output cannot be attached to the `AVCaptureSession`,
    *   The metadata output does not support QR code recognition.
2.  After a successful initialization, camera state is set to
    `CAMERA_AVAILABLE`, which is reported asynchronously to
    `QRScannerViewController`, and `CameraController` starts listening for
    camera notifications.
3.  Camera starts recording on `viewWillAppear`.

#### Torch

*   Torch availability is checked when the camera initialization completes.
*   Torch is considered available, if the properties `hasTorch` and
    `isTorchAvailable` of the `AVCaptureDevice` are both `YES`.
*   During initialization, it is also checked if the torch supports the torch
    modes `AVCaptureTorchModeOn` and `AVCaptureTorchModeOff`.
*   Torch mode is set to off on initialization.

#### Camera preview

The `AVCaptureVideoPreviewLayer` is created by the `QRScannerView`:

1.  The `QRScannerView` is initialized by the `QRScannerViewController`.
2.  On `viewDidLoad`, `QRScannerViewController` calls `loadVideoPreviewLayer:`
    with the loaded preview. If the camera is already loaded,
    `CameraController` attaches the preview to the `AVCaptureSession`. Otherwise
    the preview is attached immediately after the `AVCaptureSession` is
    initialized.

#### Viewport

The rectangle of interest for the metadata output of the capture session is
calculated to lie exactly inside the viewport drawn by the `QRScannerView`.
Resetting the viewport causes the video preview to freeze for a short while,
that is why the viewport is only set when the preview is hidden.

1.  `QRScannerViewController` sets the viewport on `viewDidAppear`, to make sure
    that the preview layer is of the correct size and position when the viewport
    is calculated.
2.  If the capture session is loaded, the viewport is set immediately. Otherwise
    the viewport is set after the capture session is loaded.
3.  `CameraController` calls the `cameraIsReady` method of its delegate to
    notify the `QRScannerViewController` that the viewport was successfully set
    and the camera preview can be displayed.

### Camera state and notifications

`CameraController` is listening for the following notifications:

1.  `AVCaptureSessionRuntimeErrorNotification`, handled by setting the camera
    state to `CAMERA_UNAVAILABLE`,
2.  `AVCaptureSessionWasInterruptedNotification`, handled by setting the camera
    state to one of:
    *   `APPLICATION_IN_BACKGROUND`,
    *   `CAMERA_IN_USE_BY_ANOTHER_APPLICATION`,
    *   `MULTIPLE_FOREGROUND_APPS`,
    based on the value of `AVCaptureSessionInterruptionReasonKey` in the
    notification's user info.
3.  `AVCaptureSessionInterruptionEndedNotification`, handled by setting the
    camera state to `CAMERA_AVAILABLE`.
4.  `AVCaptureDeviceWasDisconnected`, handled by setting the camera state to
    `CAMERA_UNAVAILABLE`.

### Torch state

The current state of the torch is obtained using key-value observing of the
`AVCaptureDevice` object.

*   Torch state is set based on the value of the `torchActive` property, and
    the delegate is informed using `torchStateChanged:`.
*   Torch availability is set based on the values of `hasTorch` and
    `torchAvailable`. Torch is only considered available if both properties are
    `YES` and the delegate is informed using `torchAvailabilityChanged:`.

The delegate sets the value of the torch using `setTorchMode:` and is informed
of the outcome asynchronously.

### Scanning

`CameraController` implements the `AVCaptureMetadataOutputObjectsDelegate` and
receives the scanned result on the main queue.

*   The scanned result must be an `AVMetadataMachineReadableCodeObject`.
*   Only results which are non-empty strings are passed on to the
    `QRScannerViewController`.
*   `CameraController` checks the type of the scanned code, and if the code can
    only contain numbers, sets the `loadImmediately` argument to `YES`.
*   If a valid code was scanned, the `CameraController` stops the capture
    session.

Supported codes (from [Machine Readable Object Types][machinereadableobjects]):

*   Numeric-only bar codes: UPC-E, EAN-8, EAN-13, Interleaved 2 of 5, ITF-14
*   Alphanumeric bar codes: Code 39, Code 39 Mod 43, Code 93, Code 128
*   2D alphanumeric codes: PDF417, AztecCode, DataMatrix, QR Code

[machinereadableobjects]: https://developer.apple.com/reference/avfoundation/avmetadatamachinereadablecodeobject/1668878-machine_readable_object_types?language=objc

## Views

The QR scanner consists of three views:

*   **VideoPreviewView** holds the camera preview.
*   **PreviewOverlayView** holds layers drawing the darker preview overlay and
    the viewport border.
*   **QRScannerView** contains the `VideoPreviewView`, `PreviewOverlayView` and
    controls as subviews.

### Transition animation

`QRScannerTransitioningDelegate` implements a custom transition animation:
the `QRScannerViewController` appears to be positioned below its presenting view
controller. The presenting view controller slides up for presentation and down
for dismissal.

### Screen rotation

*   `QRScannerView` and `PreviewOverlayView` rotate normally.
    `VideoPreviewView` does not rotate: on `viewWillTransitionToSize:` the view
    is animated to rotate in the opposite direction. This avoids resetting the
    viewport rectangle of interest on every screen rotation, because resetting
    it causes the video to pause for a while. The view is not positioned using
    AutoLayout.
*   `PreviewOverlayView` is a square that is `sqrt(2)`-times bigger than
    max(width, height) of the `QRScannerView`, to avoid redrawing the
    viewport. This also makes the viewport rotate in place. The view is
    positioned using AutoLayout.

### Split View

*   Camera is unavailable in Split View.
*   When the camera becomes available, the viewport rectangle is reset,
    otherwise the viewport would be in the wrong place when Split View is
    cancelled.

## Accessibility

*   Standard UI elements have accessibility labels and identifiers.
*   The value of the torch button is communicated using its accessibility value.
*   If VoiceOver is on, an accessibility announcement is played when a code is
    scanned, and the result is loaded after the announcement finishes.
*   If the result is loaded immediately, no additional accessibility notifications are
    posted.
*   If the result is placed in Omnibox for the user to review, the Omnibox
    should be focused afterwards.

## Metrics

The following metrics are collected:

*   `IOS.Spotlight.Action` when the user opens the QR scanner from searching for
    it in Spotlight.
*   `ApplicationShortcut.ScanQRCodePressed` when the user opens the QR scanner
    from 3D Touch application shortcuts.
*   `MobileQRScannerClose` when the user closes the QR scanner without scanning
    a code.
*   `MobileQRScannerError` when the user closes the QR scanner from an error
    dialog.
*   `MobileQRScannerScannedCode` when the user scans a code.
*   `MobileQRScannerTorchOn` when the user switches on the torch.

## Known issues

Screen rotation on iPad is not handled the same way as on iPhone, as of iOS 9.
The counter-rotation animation is not played at the same time as the screen
rotation animation, and the camera preview appears to be rotating. This effect
is most visible when rotating the screen by 180 degrees, which results in an
apparent double-rotation by 360 degrees.

## See also

*   go/chrome-ios-qr-code for the original design doc.
