// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_camera_handler.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/task/bind_post_task.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Gemini camera handler error domain.
NSString* const kGeminiCameraHandlerErrorDomain = @"GeminiCameraHandler";

}  // namespace

@interface GeminiCameraHandler () <UIImagePickerControllerDelegate,
                                   UINavigationControllerDelegate>
@end

@implementation GeminiCameraHandler {
  // The pref service used by this handler.
  raw_ptr<PrefService> _prefService;

  // Gemini camera handler completion block.
  void (^_completion)(NSArray<UIImage*>*, NSError*);

  // The presenting view controller for the camera picker and alerts.
  __weak UIViewController* _presentingViewController;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - Public

- (void)openCameraFromViewController:(UIViewController*)presentingViewController
                      withCompletion:
                          (void (^)(NSArray<UIImage*>*, NSError*))completion {
  CHECK(!_completion && !_presentingViewController, base::NotFatalUntil::M150);
  _completion = completion;
  _presentingViewController = presentingViewController;

  RecordGeminiCameraFlowBegan();

  // Ensure the hardware supports a camera.
  if (![UIImagePickerController
          isSourceTypeAvailable:UIImagePickerControllerSourceTypeCamera]) {
    [self executeCompletionWithImages:nil
                                error:[self errorWithCode:
                                                NSFeatureUnsupportedError]];
    RecordGeminiCameraFlowOSCameraAuthorizationInitialStatus(
        IOSGeminiOSCameraAuthorizationInitialStatus::kSourceTypeUnavailable);
    return;
  }

  // Camera permission check.
  AVAuthorizationStatus authStatus =
      [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];

  __weak GeminiCameraHandler* weakSelf = self;
  ProceduralBlock presentCameraBlock = ^{
    [weakSelf presentCameraPicker];
  };

  switch (authStatus) {
    case AVAuthorizationStatusAuthorized: {
      RecordGeminiCameraFlowOSCameraAuthorizationInitialStatus(
          IOSGeminiOSCameraAuthorizationInitialStatus::kAuthorized);
      [self maybeShowGeminiPermissionPromptWithCompletion:presentCameraBlock];
      break;
    }

    case AVAuthorizationStatusNotDetermined: {
      RecordGeminiCameraFlowOSCameraAuthorizationInitialStatus(
          IOSGeminiOSCameraAuthorizationInitialStatus::kNotDetermined);
      // Will start the camera picker flow on the main thread, since this can
      // be called from a background thread.
      base::OnceCallback<void(BOOL)> authorizationRequestCallback =
          base::BindPostTask(
              web::GetUIThreadTaskRunner({}), base::BindOnce(^(BOOL granted) {
                RecordGeminiCameraFlowOSAuthorizationResult(granted);
                if (!granted) {
                  [weakSelf presentGoToSettingsAlert];
                  return;
                }

                [weakSelf maybeShowGeminiPermissionPromptWithCompletion:
                              presentCameraBlock];
              }));

      [AVCaptureDevice
          requestAccessForMediaType:AVMediaTypeVideo
                  completionHandler:base::CallbackToBlock(std::move(
                                        authorizationRequestCallback))];

      break;
    }

    case AVAuthorizationStatusDenied: {
      RecordGeminiCameraFlowOSCameraAuthorizationInitialStatus(
          IOSGeminiOSCameraAuthorizationInitialStatus::kDenied);
      [self presentGoToSettingsAlert];
      break;
    }

    case AVAuthorizationStatusRestricted: {
      RecordGeminiCameraFlowOSCameraAuthorizationInitialStatus(
          IOSGeminiOSCameraAuthorizationInitialStatus::kRestricted);
      [self executeCompletionWithImages:nil
                                  error:[self errorWithCode:
                                                  NSFeatureUnsupportedError]];
      break;
    }
  }
}

#pragma mark - Private

// Checks the Gemini camera permission pref and maybe shows the Gemini-specific
// camera permission prompt.
- (void)maybeShowGeminiPermissionPromptWithCompletion:
    (ProceduralBlock)showCameraCompletion {
  if (_prefService->GetBoolean(prefs::kIOSGeminiCameraSetting)) {
    RecordGeminiCameraFlowGeminiCameraPermissionInitialValue(true);
    if (showCameraCompletion) {
      showCameraCompletion();
    }

    return;
  }

  RecordGeminiCameraFlowGeminiCameraPermissionInitialValue(false);
  [self presentGeminiPermissionAlertWithCompletion:showCameraCompletion];
}

// Presents the camera picker fullscreen on the given view controller. This
// should only be called after having checked that the Gemini-specific camera
// permission has been granted, and on the main thread.
- (void)presentCameraPicker {
  RecordGeminiCameraFlowPresentCameraPicker();

  UIImagePickerController* picker = [[UIImagePickerController alloc] init];
  picker.sourceType = UIImagePickerControllerSourceTypeCamera;
  picker.delegate = self;
  picker.modalPresentationStyle = UIModalPresentationOverFullScreen;

  [_presentingViewController presentViewController:picker
                                          animated:YES
                                        completion:nil];
}

// Executes the completion if it exists and then sets it to nil. Also sets the
// presenting view controller to nil.
- (void)executeCompletionWithImages:(NSArray<UIImage*>*)images
                              error:(NSError*)error {
  if (_completion) {
    _completion(images, error);
  }

  _completion = nil;
  _presentingViewController = nil;
}

// Returns an error with the given code and the Gemini camera handler domain.
- (NSError*)errorWithCode:(NSInteger)code {
  return [NSError errorWithDomain:kGeminiCameraHandlerErrorDomain
                             code:code
                         userInfo:nil];
}

// Presents the Gemini camera permission alert.
- (void)presentGeminiPermissionAlertWithCompletion:
    (ProceduralBlock)showCameraCompletion {
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(IDS_IOS_GEMINI_PERMISSION_CAMERA_PROMPT_TITLE)
                       message:l10n_util::GetNSString(
                                   IDS_IOS_GEMINI_PERMISSION_CAMERA_PROMPT_BODY)
                preferredStyle:UIAlertControllerStyleAlert];

  __weak GeminiCameraHandler* weakSelf = self;

  UIAlertAction* acceptAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_GRANT)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                RecordGeminiCameraFlowGeminiCameraPermissionAlertResult(true);
                [weakSelf enableGeminiCameraPermissionPref];
                if (showCameraCompletion) {
                  showCameraCompletion();
                }
              }];

  UIAlertAction* denyAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_PERMISSIONS_ALERT_DIALOG_BUTTON_TEXT_DENY)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                RecordGeminiCameraFlowGeminiCameraPermissionAlertResult(false);
                [weakSelf
                    executeCompletionWithImages:nil
                                          error:[weakSelf
                                                    errorWithCode:
                                                        NSUserCancelledError]];
              }];

  [alert addAction:acceptAction];
  [alert addAction:denyAction];

  [_presentingViewController presentViewController:alert
                                          animated:YES
                                        completion:nil];
}

// Presents the "Go to settings" alert.
- (void)presentGoToSettingsAlert {
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_GEMINI_PERMISSION_CAMERA_DISABLED_PROMPT_TITLE)
                       message:
                           l10n_util::GetNSString(
                               IDS_IOS_GEMINI_PERMISSION_CAMERA_DISABLED_PROMPT_BODY)
                preferredStyle:UIAlertControllerStyleAlert];

  __weak GeminiCameraHandler* weakSelf = self;

  UIAlertAction* acceptAction = [UIAlertAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_GEMINI_PERMISSION_CAMERA_DISABLED_PROMPT_GO_TO_SETTINGS)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                RecordGeminiCameraFlowGoToOSSettingsAlertResult(true);
                [weakSelf openAppSettings];
              }];

  UIAlertAction* denyAction = [UIAlertAction
      actionWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_GEMINI_PERMISSION_CAMERA_DISABLED_PROMPT_NO_THANKS)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                RecordGeminiCameraFlowGoToOSSettingsAlertResult(false);
                [weakSelf
                    executeCompletionWithImages:nil
                                          error:[self
                                                    errorWithCode:
                                                        NSUserCancelledError]];
              }];

  [alert addAction:acceptAction];
  [alert addAction:denyAction];

  [_presentingViewController presentViewController:alert
                                          animated:YES
                                        completion:nil];
}

// Enables Gemini camera permission at the pref level.
- (void)enableGeminiCameraPermissionPref {
  _prefService->SetBoolean(prefs::kIOSGeminiCameraSetting, true);
}

// Opens the iOS settings app.
- (void)openAppSettings {
  __weak GeminiCameraHandler* weakSelf = self;
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:UIApplicationOpenSettingsURLString]
                options:{}
      completionHandler:^(BOOL success) {
        [weakSelf
            executeCompletionWithImages:nil
                                  error:[weakSelf errorWithCode:
                                                      NSUserCancelledError]];
      }];
}

#pragma mark - UIImagePickerControllerDelegate

- (void)imagePickerController:(UIImagePickerController*)picker
    didFinishPickingMediaWithInfo:
        (NSDictionary<UIImagePickerControllerInfoKey, id>*)info {
  UIImage* image = info[UIImagePickerControllerOriginalImage];

  NSArray* results = @[];
  if (image) {
    results = @[ image ];
    RecordGeminiCameraFlowCameraPickerResult(
        IOSGeminiCameraPickerResult::kFinishedWithImage);
  } else {
    RecordGeminiCameraFlowCameraPickerResult(
        IOSGeminiCameraPickerResult::kFinishedWithoutImage);
  }

  __weak GeminiCameraHandler* weakSelf = self;
  [picker dismissViewControllerAnimated:YES
                             completion:^{
                               [weakSelf executeCompletionWithImages:results
                                                               error:nil];
                             }];
}

- (void)imagePickerControllerDidCancel:(UIImagePickerController*)picker {
  __weak GeminiCameraHandler* weakSelf = self;
  ProceduralBlock cameraDismissedBlock = ^{
    [weakSelf
        executeCompletionWithImages:nil
                              error:[weakSelf
                                        errorWithCode:NSUserCancelledError]];
  };

  RecordGeminiCameraFlowCameraPickerResult(
      IOSGeminiCameraPickerResult::kCancelled);

  [picker dismissViewControllerAnimated:YES completion:cameraDismissedBlock];
}

@end
