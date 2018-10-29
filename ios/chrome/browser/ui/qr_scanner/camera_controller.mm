// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/qr_scanner/camera_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/stringprintf.h"
#include "ios/chrome/common/ios_app_bundle_id_prefix_buildflags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CameraController ()<AVCaptureMetadataOutputObjectsDelegate> {
  // The capture session for recording video and detecting QR codes.
  AVCaptureSession* _captureSession;
  // The metadata output attached to the capture session.
  AVCaptureMetadataOutput* _metadataOutput;
  // The delegate which receives the scanned result. All methods of this
  // delegate should be called on the main queue.
  __weak id<CameraControllerDelegate> _delegate;
  // The queue for dispatching calls to |_captureSession|.
  dispatch_queue_t _sessionQueue;
}

// The current state of the camera. The state is set to CAMERA_NOT_LOADED before
// the camera is first loaded, and afterwards it is never CAMERA_NOT_LOADED.
@property(nonatomic, readwrite, assign) qr_scanner::CameraState cameraState;
// Redeclaration of |torchActive| to make the setter private.
@property(nonatomic, readwrite, assign, getter=isTorchActive) BOOL torchActive;
// The current availability of the torch.
@property(nonatomic, readwrite, assign, getter=isTorchAvailable)
    BOOL torchAvailable;

// Initializes the controller with the |delegate|.
- (instancetype)initWithDelegate:(id<CameraControllerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// YES if |cameraState| is CAMERA_AVAILABLE.
- (BOOL)isCameraAvailable;
// Starts receiving notfications about changes to the capture session and to the
// torch properties.
- (void)startReceivingNotifications;
// Stops receiving all notifications.
- (void)stopReceivingNotifications;
// Returns the camera attached to |_captureSession|.
- (AVCaptureDevice*)getCamera;
// Returns the AVCaptureVideoOrientation to compensate for the current
// UIInterfaceOrientation. Defaults to AVCaptureVideoOrientationPortrait.
- (AVCaptureVideoOrientation)videoOrientationForCurrentInterfaceOrientation;

@end

@implementation CameraController {
  qr_scanner::CameraState _cameraState;
  BOOL _torchActive;
  BOOL _torchAvailable;
  CGRect _viewportRect;
}

#pragma mark lifecycle

+ (instancetype)cameraControllerWithDelegate:
    (id<CameraControllerDelegate>)delegate {
  CameraController* cameraController =
      [[CameraController alloc] initWithDelegate:delegate];
  return cameraController;
}

- (instancetype)initWithDelegate:(id<CameraControllerDelegate>)delegate {
  self = [super init];
  if (self) {
    DCHECK(delegate);
    _cameraState = qr_scanner::CAMERA_NOT_LOADED;
    _delegate = delegate;
    std::string queueName =
        base::StringPrintf("%s.chrome.ios.QRScannerCaptureSessionQueue",
                           BUILDFLAG(IOS_APP_BUNDLE_ID_PREFIX));
    _sessionQueue =
        dispatch_queue_create(queueName.c_str(), DISPATCH_QUEUE_SERIAL);
    _torchAvailable = NO;
    _torchActive = NO;
    _viewportRect = CGRectNull;
  }
  return self;
}

- (void)dealloc {
  [self stopReceivingNotifications];
}

#pragma mark public methods

- (AVAuthorizationStatus)getAuthorizationStatus {
  return [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
}

- (void)requestAuthorizationAndLoadCaptureSession:
    (AVCaptureVideoPreviewLayer*)previewLayer {
  DCHECK(previewLayer);
  DCHECK([self getAuthorizationStatus] == AVAuthorizationStatusNotDetermined);
  [AVCaptureDevice
      requestAccessForMediaType:AVMediaTypeVideo
              completionHandler:^void(BOOL granted) {
                if (!granted) {
                  [self setCameraState:qr_scanner::CAMERA_PERMISSION_DENIED];
                } else {
                  [self loadCaptureSession:previewLayer];
                }
              }];
}

- (void)setViewport:(CGRect)viewportRect {
  dispatch_async(_sessionQueue, ^{
    _viewportRect = viewportRect;
    if (_metadataOutput) {
      [_metadataOutput setRectOfInterest:_viewportRect];
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
  dispatch_async(_sessionQueue, ^{
    if ([self isCameraAvailable]) {
      if (![_captureSession isRunning]) {
        [_captureSession startRunning];
      }
    }
  });
}

- (void)stopRecording {
  dispatch_async(_sessionQueue, ^{
    if ([self isCameraAvailable]) {
      if ([_captureSession isRunning]) {
        [_captureSession stopRunning];
      }
    }
  });
}

- (void)setTorchMode:(AVCaptureTorchMode)mode {
  dispatch_async(_sessionQueue, ^{
    if (![self isCameraAvailable]) {
      return;
    }
    AVCaptureDevice* camera = [self getCamera];
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

#pragma mark private methods

- (BOOL)isCameraAvailable {
  return [self cameraState] == qr_scanner::CAMERA_AVAILABLE;
}

- (void)loadCaptureSession:(AVCaptureVideoPreviewLayer*)previewLayer {
  DCHECK(previewLayer);
  DCHECK([self cameraState] == qr_scanner::CAMERA_NOT_LOADED);
  DCHECK([self getAuthorizationStatus] == AVAuthorizationStatusAuthorized);
  dispatch_async(_sessionQueue, ^{
    // Get the back camera.
    NSArray* videoCaptureDevices = nil;

    // Although Apple documentation claims that
    // AVCaptureDeviceDiscoverySession etc. is available on iOS 10+, they are
    // not really available on an app whose deployment target is iOS 10.0
    // (iOS 10.1+ are okay) and Chrome will fail at dynamic link time and
    // instantly crash.  NSClassFromString() checks if Objective-C run-time
    // has the classes before using them.
    Class discoverSessionClass =
        NSClassFromString(@"AVCaptureDeviceDiscoverySession");
    if (discoverSessionClass) {
      // Hardcoded value of AVCaptureDeviceTypeBuiltInWideAngleCamera here.
      // When this @available(iOS 10, *) is deprecated, the unit test
      // CameraControllerTest.TestAVCaptureDeviceValue can be removed.
      // See https://crbug.com/826011
      NSString* cameraType = @"AVCaptureDeviceTypeBuiltInWideAngleCamera";
      AVCaptureDeviceDiscoverySession* discoverySession = [discoverSessionClass
          discoverySessionWithDeviceTypes:@[ cameraType ]
                                mediaType:AVMediaTypeVideo
                                 position:AVCaptureDevicePositionBack];
      videoCaptureDevices = [discoverySession devices];
    }
    if ([videoCaptureDevices count] == 0) {
      [self setCameraState:qr_scanner::CAMERA_UNAVAILABLE];
      return;
    }

    NSUInteger cameraIndex = [videoCaptureDevices
        indexOfObjectPassingTest:^BOOL(AVCaptureDevice* device, NSUInteger idx,
                                       BOOL* stop) {
          return device.position == AVCaptureDevicePositionBack;
        }];

    // Allow only the back camera.
    if (cameraIndex == NSNotFound) {
      [self setCameraState:qr_scanner::CAMERA_UNAVAILABLE];
      return;
    }
    AVCaptureDevice* camera = videoCaptureDevices[cameraIndex];

    // Configure camera input.
    NSError* error = nil;
    AVCaptureDeviceInput* videoInput =
        [AVCaptureDeviceInput deviceInputWithDevice:camera error:&error];
    if (error || !videoInput) {
      [self setCameraState:qr_scanner::CAMERA_UNAVAILABLE];
      return;
    }

    AVCaptureSession* session = [[AVCaptureSession alloc] init];
    if (![session canAddInput:videoInput]) {
      [self setCameraState:qr_scanner::CAMERA_UNAVAILABLE];
      return;
    }
    [session addInput:videoInput];

    // Configure metadata output.
    AVCaptureMetadataOutput* metadataOutput =
        [[AVCaptureMetadataOutput alloc] init];
    [metadataOutput setMetadataObjectsDelegate:self
                                         queue:dispatch_get_main_queue()];
    if (![session canAddOutput:metadataOutput]) {
      [self setCameraState:qr_scanner::CAMERA_UNAVAILABLE];
      return;
    }
    [session addOutput:metadataOutput];
    NSArray* availableCodeTypes = [metadataOutput availableMetadataObjectTypes];

    // Require QR code recognition to be available.
    if (![availableCodeTypes containsObject:AVMetadataObjectTypeQRCode]) {
      [self setCameraState:qr_scanner::CAMERA_UNAVAILABLE];
      return;
    }
    [metadataOutput setMetadataObjectTypes:availableCodeTypes];
    _metadataOutput = metadataOutput;

    _captureSession = session;
    [self setCameraState:qr_scanner::CAMERA_AVAILABLE];
    // Setup torchAvailable.
    [self
        setTorchAvailable:[camera hasTorch] &&
                          [camera isTorchModeSupported:AVCaptureTorchModeOn] &&
                          [camera isTorchModeSupported:AVCaptureTorchModeOff]];

    [previewLayer setSession:_captureSession];
    [previewLayer setVideoGravity:AVLayerVideoGravityResizeAspectFill];
    dispatch_async(dispatch_get_main_queue(), ^{
      [self resetVideoOrientation:previewLayer];
      [_delegate captureSessionIsConnected];
      [self startRecording];
    });
    [self startReceivingNotifications];
  });
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
  AVCaptureDevice* camera = [self getCamera];
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
}

- (void)stopReceivingNotifications {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  AVCaptureDevice* camera = [self getCamera];
  [camera removeObserver:self forKeyPath:@"hasTorch"];
  [camera removeObserver:self forKeyPath:@"torchAvailable"];
  [camera removeObserver:self forKeyPath:@"torchActive"];
}

- (AVCaptureDevice*)getCamera {
  AVCaptureDeviceInput* captureSessionInput =
      [[_captureSession inputs] firstObject];
  DCHECK(captureSessionInput != nil);
  return [captureSessionInput device];
}

- (AVCaptureVideoOrientation)videoOrientationForCurrentInterfaceOrientation {
  UIInterfaceOrientation orientation =
      [[UIApplication sharedApplication] statusBarOrientation];
  switch (orientation) {
    case UIInterfaceOrientationUnknown:
      return AVCaptureVideoOrientationPortrait;
    default:
      return static_cast<AVCaptureVideoOrientation>(orientation);
  }
}

#pragma mark notification handlers

- (void)handleAVCaptureSessionRuntimeError:(NSNotification*)notification {
  dispatch_async(_sessionQueue, ^{
    [self setCameraState:qr_scanner::CAMERA_UNAVAILABLE];
  });
}

- (void)handleAVCaptureSessionWasInterrupted:(NSNotification*)notification {
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
        [self setCameraState:qr_scanner::CAMERA_IN_USE_BY_ANOTHER_APPLICATION];
        break;
      case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableWithMultipleForegroundApps:
        [self setCameraState:qr_scanner::MULTIPLE_FOREGROUND_APPS];
        break;
      case AVCaptureSessionInterruptionReasonVideoDeviceNotAvailableDueToSystemPressure:
        [self setCameraState:qr_scanner::CAMERA_UNAVAILABLE_DUE_TO_SYSTEM_PRESSURE];
        break;
      case AVCaptureSessionInterruptionReasonAudioDeviceInUseByAnotherClient:
        NOTREACHED();
        break;
    }
  });
}

- (void)handleAVCaptureSessionInterruptionEnded:(NSNotification*)notification {
  dispatch_async(_sessionQueue, ^{
    if ([_captureSession isRunning]) {
      [self setCameraState:qr_scanner::CAMERA_AVAILABLE];
    }
  });
}

- (void)handleAVCaptureDeviceWasDisconnected:(NSNotification*)notification {
  dispatch_async(_sessionQueue, ^{
    [self setCameraState:qr_scanner::CAMERA_UNAVAILABLE];
  });
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSString*, id>*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"hasTorch"] ||
      [keyPath isEqualToString:@"torchAvailable"] ||
      [keyPath isEqualToString:@"torchActive"]) {
    AVCaptureDevice* camera = [self getCamera];
    [self setTorchAvailable:([camera hasTorch] && [camera isTorchAvailable])];
    [self setTorchActive:[camera isTorchActive]];
  }
}

#pragma mark property implementation

- (qr_scanner::CameraState)cameraState {
  return _cameraState;
}

- (void)setCameraState:(qr_scanner::CameraState)state {
  if (state == _cameraState) {
    return;
  }
  _cameraState = state;
  dispatch_async(dispatch_get_main_queue(), ^{
    [_delegate cameraStateChanged:state];
  });
}

- (BOOL)isTorchAvailable {
  return _torchAvailable;
}

- (void)setTorchAvailable:(BOOL)available {
  if (available == _torchAvailable) {
    return;
  }
  _torchAvailable = available;
  dispatch_async(dispatch_get_main_queue(), ^{
    [_delegate torchAvailabilityChanged:available];
  });
}

- (BOOL)isTorchActive {
  return _torchActive;
}

- (void)setTorchActive:(BOOL)active {
  if (active == _torchActive) {
    return;
  }
  _torchActive = active;
  dispatch_async(dispatch_get_main_queue(), ^{
    [_delegate torchStateChanged:active];
  });
}

#pragma mark AVCaptureMetadataOutputObjectsDelegate

- (void)captureOutput:(AVCaptureOutput*)captureOutput
    didOutputMetadataObjects:(NSArray*)metadataObjects
              fromConnection:(AVCaptureConnection*)connection {
  AVMetadataObject* metadataResult = [metadataObjects firstObject];
  if (![metadataResult
          isKindOfClass:[AVMetadataMachineReadableCodeObject class]]) {
    return;
  }
  NSString* resultString =
      [base::mac::ObjCCastStrict<AVMetadataMachineReadableCodeObject>(
          metadataResult) stringValue];
  if (resultString.length == 0) {
    return;
  }

  dispatch_async(_sessionQueue, ^{
    if ([_captureSession isRunning]) {
      [_captureSession stopRunning];
    }
  });

  // Check if the barcode can only contain digits. In this case, the result can
  // be loaded immediately.
  NSString* resultType = metadataResult.type;
  BOOL isAllDigits =
      [resultType isEqualToString:AVMetadataObjectTypeUPCECode] ||
      [resultType isEqualToString:AVMetadataObjectTypeEAN8Code] ||
      [resultType isEqualToString:AVMetadataObjectTypeEAN13Code] ||
      [resultType isEqualToString:AVMetadataObjectTypeInterleaved2of5Code] ||
      [resultType isEqualToString:AVMetadataObjectTypeITF14Code];

  // Note: |captureOutput| is called on the main queue. This is specified by
  // |setMetadataObjectsDelegate:queue:|.
  [_delegate receiveQRScannerResult:resultString loadImmediately:isAllDigits];
}

@end
