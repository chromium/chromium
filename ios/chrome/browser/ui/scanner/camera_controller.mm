// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scanner/camera_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/stringprintf.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ios_app_bundle_id_prefix_buildflags.h"

@interface CameraController ()

// The queue for dispatching calls to `_captureSession`.
@property(nonatomic, readonly) dispatch_queue_t sessionQueue;
// The capture session for recording video and detecting QR codes.
@property(nonatomic, readwrite) AVCaptureSession* captureSession;
// The metadata output attached to the capture session.
@property(nonatomic, readwrite) AVCaptureMetadataOutput* metadataOutput;
// The delegate which receives the scanned result. All methods of this
// delegate should be called on the main queue.
@property(nonatomic, readwrite, weak) id<CameraControllerDelegate> delegate;
// The current state of the camera. The state is set to CAMERA_NOT_LOADED before
// the camera is first loaded, and afterwards it is never CAMERA_NOT_LOADED.
@property(nonatomic, readwrite, assign) scanner::CameraState cameraState;
// Redeclaration of `torchActive` to make the setter private.
@property(nonatomic, readwrite, assign, getter=isTorchActive) BOOL torchActive;
// The current availability of the torch.
@property(nonatomic, readwrite, assign, getter=isTorchAvailable)
    BOOL torchAvailable;
// The state of KVO for the camera. Used to stop observing on dealloc.
@property(nonatomic, readwrite, assign, getter=isObservingCamera)
    BOOL observingCamera;
@property(nonatomic, readwrite, assign) CGRect viewportRect;

// YES if `cameraState` is CAMERA_AVAILABLE.
- (BOOL)isCameraAvailable;
// Starts receiving notfications about changes to the capture session and to the
// torch properties.
- (void)startReceivingNotifications;
// Stops receiving all notifications.
- (void)stopReceivingNotifications;
// Returns the camera attached to `_captureSession`.
- (AVCaptureDevice*)camera;
// Returns the AVCaptureVideoOrientation to compensate for the current
// UIInterfaceOrientation. Defaults to AVCaptureVideoOrientationPortrait.
- (AVCaptureVideoOrientation)videoOrientationForCurrentInterfaceOrientation;

@end

@implementation CameraController

#pragma mark - Lifecycle

- (instancetype)initWithDelegate:(id<CameraControllerDelegate>)delegate {
  self = [super init];
  if (self) {
    DCHECK(delegate);
    _cameraState = scanner::CAMERA_NOT_LOADED;
    _delegate = delegate;
    std::string queueName =
        base::StringPrintf("%s.chrome.ios.QRScannerCaptureSessionQueue",
                           BUILDFLAG(IOS_APP_BUNDLE_ID_PREFIX));
    _sessionQueue =
        dispatch_queue_create(queueName.c_str(), DISPATCH_QUEUE_SERIAL);
    self.torchAvailable = NO;
    self.torchActive = NO;
    self.viewportRect = CGRectNull;
  }
  return self;
}

- (void)dealloc {
  [self stopReceivingNotifications];
}

#pragma mark - Public methods

- (AVAuthorizationStatus)authorizationStatus {
  return [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
}

- (void)requestAuthorizationAndLoadCaptureSession:
    (AVCaptureVideoPreviewLayer*)previewLayer {
  DCHECK(previewLayer);
  DCHECK([self authorizationStatus] == AVAuthorizationStatusNotDetermined);
  __weak CameraController* weakSelf = self;
  [AVCaptureDevice
      requestAccessForMediaType:AVMediaTypeVideo
              completionHandler:^void(BOOL granted) {
                if (!granted) {
                  [weakSelf setCameraState:scanner::CAMERA_PERMISSION_DENIED];
                } else {
                  [weakSelf loadCaptureSession:previewLayer];
                }
              }];
}

- (void)setViewport:(CGRect)viewportRect {
  __weak CameraController* weakSelf = self;
  dispatch_async(_sessionQueue, ^{
    CameraController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    strongSelf.viewportRect = viewportRect;
    if (strongSelf.metadataOutput) {
      [strongSelf.metadataOutput setRectOfInterest:strongSelf->_viewportRect];
    }
  });
}

- (void)resetVideoOrientation:(AVCaptureVideoPreviewLayer*)previewLayer {
  DCHECK(previewLayer);
  AVCaptureConnection* videoConnection = [previewLayer connection];
  if ([videoConnection isVideoOrientationSupported]) {
    [videoConnection setVideoOrientation:
                         [self videoOrientationForCurrentInterfaceOrientation]];
  }
}

- (void)startRecording {
  __weak CameraController* weakSelf = self;
  dispatch_async(_sessionQueue, ^{
    CameraController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    if ([strongSelf isCameraAvailable]) {
      if (![strongSelf.captureSession isRunning]) {
        [strongSelf.captureSession startRunning];
      }
    }
  });
}

- (void)stopRecording {
  __weak CameraController* weakSelf = self;
  dispatch_async(_sessionQueue, ^{
    CameraController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    if ([strongSelf isCameraAvailable] &&
        [strongSelf.captureSession isRunning]) {
      [strongSelf.captureSession stopRunning];
    }
  });
}

- (void)setTorchMode:(AVCaptureTorchMode)mode {
  __weak CameraController* weakSelf = self;
  dispatch_async(_sessionQueue, ^{
    CameraController* strongSelf = weakSelf;
    if (!strongSelf || ![strongSelf isCameraAvailable]) {
      return;
    }
    AVCaptureDevice* camera = [strongSelf camera];
    if (![camera isTorchModeSupported:mode]) {
      return;
    }
    NSError* error = nil;
    [camera lockForConfiguration:&error];
    if (error) {
      return;
    }
    [camera setTorchMode:mode];
    [camera unlockForConfiguration];
  });
}

#pragma mark - Private methods

- (BOOL)isCameraAvailable {
  return [self cameraState] == scanner::CAMERA_AVAILABLE;
}

- (void)loadCaptureSession:(AVCaptureVideoPreviewLayer*)previewLayer {
  DCHECK(previewLayer);
  DCHECK([self cameraState] == scanner::CAMERA_NOT_LOADED);
  DCHECK([self authorizationStatus] == AVAuthorizationStatusAuthorized);
  __weak CameraController* weakSelf = self;
  dispatch_async(_sessionQueue, ^{
    [weakSelf continueLoadCaptureSession:previewLayer];
  });
}

- (void)continueLoadCaptureSession:(AVCaptureVideoPreviewLayer*)previewLayer {
  // Get the back camera.
  NSArray* videoCaptureDevices = nil;
  NSString* cameraType = AVCaptureDeviceTypeBuiltInWideAngleCamera;
  AVCaptureDeviceDiscoverySession* discoverySession =
      [AVCaptureDeviceDiscoverySession
          discoverySessionWithDeviceTypes:@[ cameraType ]
                                mediaType:AVMediaTypeVideo
                                 position:AVCaptureDevicePositionBack];
  videoCaptureDevices = [discoverySession devices];
  if ([videoCaptureDevices count] == 0) {
    [self setCameraState:scanner::CAMERA_UNAVAILABLE];
    return;
  }

  NSUInteger cameraIndex = [videoCaptureDevices
      indexOfObjectPassingTest:^BOOL(AVCaptureDevice* device, NSUInteger idx,
                                     BOOL* stop) {
        return device.position == AVCaptureDevicePositionBack;
      }];

  // Allow only the back camera.
  if (cameraIndex == NSNotFound) {
    [self setCameraState:scanner::CAMERA_UNAVAILABLE];
    return;
  }
  AVCaptureDevice* camera = videoCaptureDevices[cameraIndex];

  // Configure camera input.
  NSError* error = nil;
  AVCaptureDeviceInput* videoInput =
      [AVCaptureDeviceInput deviceInputWithDevice:camera error:&error];
  if (error || !videoInput) {
    [self setCameraState:scanner::CAMERA_UNAVAILABLE];
    return;
  }

  AVCaptureSession* session = [[AVCaptureSession alloc] init];
  if (![session canAddInput:videoInput]) {
    [self setCameraState:scanner::CAMERA_UNAVAILABLE];
    return;
  }
  [session addInput:videoInput];

  [self configureScannerWithSession:session];

  _captureSession = session;
  [self setCameraState:scanner::CAMERA_AVAILABLE];
  // Setup torchAvailable.
  [self setTorchAvailable:[camera hasTorch] &&
                          [camera isTorchModeSupported:AVCaptureTorchModeOn] &&
                          [camera isTorchModeSupported:AVCaptureTorchModeOff]];

  [previewLayer setSession:_captureSession];
  [previewLayer setVideoGravity:AVLayerVideoGravityResizeAspectFill];
  __weak CameraController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf
        captureSessionConnected:(AVCaptureVideoPreviewLayer*)previewLayer];
  });
  [self startReceivingNotifications];
}

- (void)captureSessionConnected:(AVCaptureVideoPreviewLayer*)previewLayer {
  [self resetVideoOrientation:previewLayer];
  [_delegate captureSessionIsConnected];
  [self startRecording];
}

- (void)configureScannerWithSession:(AVCaptureSession*)session {
  NOTREACHED_IN_MIGRATION();
}

- (void)startReceivingNotifications {
  // Start receiving notifications about changes to the capture session.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleAVCaptureSessionRuntimeError:)
             name:AVCaptureSessionRuntimeErrorNotification
           object:_captureSession];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleAVCaptureSessionWasInterrupted:)
             name:AVCaptureSessionWasInterruptedNotification
           object:_captureSession];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleAVCaptureSessionInterruptionEnded:)
             name:AVCaptureSessionInterruptionEndedNotification
           object:_captureSession];

  // Start receiving notifications about changes to the camera.
  AVCaptureDevice* camera = [self camera];
  DCHECK(camera);

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleAVCaptureDeviceWasDisconnected:)
             name:AVCaptureDeviceWasDisconnectedNotification
           object:camera];

  // Start receiving notifications about changes to the torch state.
  [camera addObserver:self
           forKeyPath:@"hasTorch"
              options:NSKeyValueObservingOptionNew
              context:nil];

  [camera addObserver:self
           forKeyPath:@"torchAvailable"
              options:NSKeyValueObservingOptionNew
              context:nil];

  [camera addObserver:self
           forKeyPath:@"torchActive"
              options:NSKeyValueObservingOptionNew
              context:nil];
  self.observingCamera = YES;
}

- (void)stopReceivingNotifications {
  // We only start receiving notifications if the camera is available.
  if ([self isObservingCamera]) {
    AVCaptureDevice* camera = [self camera];
    [camera removeObserver:self forKeyPath:@"hasTorch"];
    [camera removeObserver:self forKeyPath:@"torchAvailable"];
    [camera removeObserver:self forKeyPath:@"torchActive"];
  }
}

- (AVCaptureDevice*)camera {
  AVCaptureDeviceInput* captureSessionInput =
      [[_captureSession inputs] firstObject];
  DCHECK(captureSessionInput != nil);
  return [captureSessionInput device];
}

- (AVCaptureVideoOrientation)videoOrientationForCurrentInterfaceOrientation {
  UIInterfaceOrientation orientation = GetInterfaceOrientation();
  switch (orientation) {
    case UIInterfaceOrientationUnknown:
      return AVCaptureVideoOrientationPortrait;
    default:
      return static_cast<AVCaptureVideoOrientation>(orientation);
  }
}

#pragma mark - Notification Handlers

- (void)handleAVCaptureSessionRuntimeError:(NSNotification*)notification {
  __weak CameraController* weakSelf = self;
  dispatch_async(_sessionQueue, ^{
    [weakSelf setCameraState:scanner::CAMERA_UNAVAILABLE];
  });
}

- (void)handleAVCaptureSessionWasInterrupted:(NSNotification*)notification {
  __weak CameraController* weakSelf = self;
  dispatch_async(_sessionQueue, ^{
    AVCaptureSessionInterruptionReason reason =
        (AVCaptureSessionInterruptionReason)[[[notification userInfo]
            valueForKey:AVCaptureSessionInterruptionReasonKey] integerValue];
    switch (reason) {
      case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableInBackground:
        // iOS automatically stops and restarts capture sessions when the app
        // is backgrounded and foregrounded.
        break;
      case AVCaptureSessionInterruptionReasonVideoDeviceInUseByAnotherClient:
        [weakSelf setCameraState:scanner::CAMERA_IN_USE_BY_ANOTHER_APPLICATION];
        break;
      case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps:
        [weakSelf setCameraState:scanner::MULTIPLE_FOREGROUND_APPS];
        break;
      case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableDueToSystemPressure:
        [weakSelf
            setCameraState:scanner::CAMERA_UNAVAILABLE_DUE_TO_SYSTEM_PRESSURE];
        break;
      case AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  });
}

- (void)handleAVCaptureSessionInterruptionEnded:(NSNotification*)notification {
  __weak CameraController* weakSelf = self;
  dispatch_async(_sessionQueue, ^{
    CameraController* strongSelf = weakSelf;
    if (strongSelf && [strongSelf.captureSession isRunning]) {
      [strongSelf setCameraState:scanner::CAMERA_AVAILABLE];
    }
  });
}

- (void)handleAVCaptureDeviceWasDisconnected:(NSNotification*)notification {
  __weak CameraController* weakSelf = self;
  dispatch_async(_sessionQueue, ^{
    [weakSelf setCameraState:scanner::CAMERA_UNAVAILABLE];
  });
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSString*, id>*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"hasTorch"] ||
      [keyPath isEqualToString:@"torchAvailable"] ||
      [keyPath isEqualToString:@"torchActive"]) {
    AVCaptureDevice* camera = [self camera];
    [self setTorchAvailable:([camera hasTorch] && [camera isTorchAvailable])];
    [self setTorchActive:[camera isTorchActive]];
  }
}

#pragma mark - Property Implementation

- (void)setCameraState:(scanner::CameraState)state {
  if (state == _cameraState) {
    return;
  }
  _cameraState = state;
  __weak CameraController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.delegate cameraStateChanged:state];
  });
}

- (void)setTorchAvailable:(BOOL)available {
  if (available == _torchAvailable) {
    return;
  }
  _torchAvailable = available;
  __weak CameraController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.delegate torchAvailabilityChanged:available];
  });
}

- (void)setTorchActive:(BOOL)active {
  if (active == _torchActive) {
    return;
  }
  _torchActive = active;
  __weak CameraController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.delegate torchStateChanged:active];
  });
}

@end
