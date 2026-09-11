// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/image/image_saver.h"

#import <Photos/Photos.h>

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface ImageSaver ()
// Base view controller for the alerts.
@property(nonatomic, weak) UIViewController* baseViewController;
@property(nonatomic, readonly) Browser* browser;
@end

@implementation ImageSaver {
  // Alert to give feedback to the user.
  UIAlertController* _alertController;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
  }
  return self;
}

- (void)stop {
  [self dismissAlert];
  self.baseViewController = nil;
  _browser = nullptr;
}

- (void)saveImageAtURL:(const GURL&)URL
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState
               frameID:(const std::string&)frameID
           frameOrigin:(const url::Origin&)frameOrigin
    baseViewController:(UIViewController*)baseViewController {
  self.baseViewController = baseViewController;

  ImageFetchTabHelper* tabHelper = ImageFetchTabHelper::FromWebState(webState);
  DCHECK(tabHelper);

  __weak ImageSaver* weakSelf = self;
  tabHelper->GetImageData(URL, referrer, frameID, frameOrigin, ^(NSData* data) {
    [weakSelf didGetImageData:data];
  });
}

#pragma mark - Private

// Callback when the image `data` got retrieved from the tab.
- (void)didGetImageData:(NSData*)data {
  if (data.length == 0) {
    [self
        displayPrivacyErrorAlertOnMainQueue:
            l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_NO_INTERNET_CONNECTION)];
    return;
  }

  // Dump `data` into the photo library. Handing raw data directly to
  // PHPhotoLibrary avoids in-process image decoding while preserving the
  // original format and metadata.
  __weak ImageSaver* weakSelf = self;
  [[PHPhotoLibrary sharedPhotoLibrary]
      performChanges:^{
        PHAssetResourceCreationOptions* options =
            [[PHAssetResourceCreationOptions alloc] init];
        [[PHAssetCreationRequest creationRequestForAsset]
            addResourceWithType:PHAssetResourceTypePhoto
                           data:data
                        options:options];
      }
      completionHandler:^(BOOL success, NSError* error) {
        [weakSelf didFinishSavingWithError:error];
      }];
}

// Called when Chrome has been denied access to add photos or videos and the
// user can change it.
// Shows a privacy alert on the main queue, allowing the user to go to Chrome's
// settings. Dismiss previous alert if it has not been dismissed yet.
- (void)displayImageErrorAlertWithSettingsOnMainQueue {
  __weak ImageSaver* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    NSURL* settingURL =
        [NSURL URLWithString:UIApplicationOpenSettingsURLString];
    BOOL canGoToSetting =
        [[UIApplication sharedApplication] canOpenURL:settingURL];
    if (canGoToSetting) {
      [weakSelf displayImageErrorAlertWithSettings:settingURL];
    } else {
      [weakSelf
          displayPrivacyErrorAlertOnMainQueue:
              l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_MESSAGE)];
    }
  });
}

// Shows a privacy alert allowing the user to go to Chrome's settings. Dismiss
// previous alert if it has not been dismissed yet.
- (void)displayImageErrorAlertWithSettings:(NSURL*)settingURL {
  [self dismissAlert];

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_MESSAGE_GO_TO_SETTINGS);
  _alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  [_alertController
      addAction:[UIAlertAction
                    actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                              style:UIAlertActionStyleCancel
                            handler:nil]];

  UIAlertAction* openSettings = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_GO_TO_SETTINGS)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                [[UIApplication sharedApplication] openURL:settingURL
                                                   options:@{}
                                         completionHandler:nil];
              }];
  [_alertController addAction:openSettings];

  [self.baseViewController presentViewController:_alertController
                                        animated:YES
                                      completion:nil];
}

// Called when Chrome has been denied access to the photos or videos and the
// user cannot change it.
- (void)displayPrivacyErrorAlertOnMainQueue:(NSString*)errorContent {
  __weak ImageSaver* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf asyncDisplayPrivacyErrorAlertOnMainQueue:errorContent];
  });
}

// Async helper implementation of displayPrivacyErrorAlertOnMainQueue.
// Shows a privacy alert on the main queue, with errorContent as the message.
// Dismisses previous alert if it has not been dismissed yet.
- (void)asyncDisplayPrivacyErrorAlertOnMainQueue:(NSString*)errorContent {
  [self dismissAlert];

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE);
  _alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:errorContent
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  okAction.accessibilityIdentifier =
      [l10n_util::GetNSString(IDS_OK) stringByAppendingString:@"AlertAction"];
  [_alertController addAction:okAction];

  [self.baseViewController presentViewController:_alertController
                                        animated:YES
                                      completion:nil];
}

// Called after the system attempts to write the image to the photo library.
- (void)didFinishSavingWithError:(NSError*)error {
  // Was there an error?
  if (error) {
    // Check if saving failed due to insufficient permissions.
    // This code may be executed outside of the main thread. Make sure to
    // display the error on the main thread.
    PHAuthorizationStatus status =
        [PHPhotoLibrary authorizationStatusForAccessLevel:PHAccessLevelAddOnly];
    if (status == PHAuthorizationStatusDenied ||
        status == PHAuthorizationStatusRestricted) {
      [self displayImageErrorAlertWithSettingsOnMainQueue];
    } else {
      [self displayPrivacyErrorAlertOnMainQueue:l10n_util::GetNSString(
                                                    IDS_IOS_SAVE_IMAGE_ERROR)];
    }
  } else {
    // TODO(crbug.com/41362123): Provide a way for the user to easily reach the
    // photos app.
  }
}

// Dismisses the alert.
- (void)dismissAlert {
  [_alertController.presentingViewController dismissViewControllerAnimated:YES
                                                                completion:nil];
  _alertController = nil;
}

@end
