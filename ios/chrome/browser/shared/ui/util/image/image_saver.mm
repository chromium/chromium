// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/image/image_saver.h"

#import <Photos/Photos.h>

#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/format_macros.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mime_util.h"
#import "ui/base/l10n/l10n_util.h"

@interface ImageSaver ()
// Base view controller for the alerts.
@property(nonatomic, weak) UIViewController* baseViewController;
// Alert coordinator to give feedback to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
@property(nonatomic, readonly) Browser* browser;
@end

@implementation ImageSaver

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _browser = browser;
  }
  return self;
}

- (void)stop {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
  self.baseViewController = nil;
  _browser = nullptr;
}

- (void)saveImageAtURL:(const GURL&)URL
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState
    baseViewController:(UIViewController*)baseViewController {
  self.baseViewController = baseViewController;

  ImageFetchTabHelper* tabHelper = ImageFetchTabHelper::FromWebState(webState);
  DCHECK(tabHelper);

  __weak ImageSaver* weakSelf = self;
  tabHelper->GetImageData(URL, referrer, ^(NSData* data) {
    [weakSelf didGetImageData:data];
  });
}

// Callback when the image `data` got retrieved from the tab.
- (void)didGetImageData:(NSData*)data {
  if (data.length == 0) {
    [self
        displayPrivacyErrorAlertOnMainQueue:
            l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_NO_INTERNET_CONNECTION)];
    return;
  }

  // Use -imageWithData to validate `data`, but continue to pass the raw
  // `data` to -savePhoto to ensure no data loss occurs.
  UIImage* savedImage = [UIImage imageWithData:data];
  if (!savedImage) {
    [self displayPrivacyErrorAlertOnMainQueue:l10n_util::GetNSString(
                                                  IDS_IOS_SAVE_IMAGE_ERROR)];
    return;
  }

  // Dump `data` into the photo library. Requires the usage of
  // NSPhotoLibraryAddUsageDescription.
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
        [weakSelf image:savedImage
            didFinishSavingWithError:error
                         contextInfo:nil];
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
  // Dismiss current alert.
  [_alertCoordinator stop];

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_MESSAGE_GO_TO_SETTINGS);

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:_browser
                           title:title
                         message:message];

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                   action:nil
                                    style:UIAlertActionStyleCancel];

  [_alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_GO_TO_SETTINGS)
                action:^{
                  [[UIApplication sharedApplication] openURL:settingURL
                                                     options:@{}
                                           completionHandler:nil];
                }
                 style:UIAlertActionStyleDefault];

  [_alertCoordinator start];
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
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE);
  // Dismiss current alert.
  [self.alertCoordinator stop];

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:_browser
                           title:title
                         message:errorContent];
  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                                   action:nil
                                    style:UIAlertActionStyleDefault];
  [self.alertCoordinator start];
}

// Called after the system attempts to write the image to the saved photos
// album.
- (void)image:(UIImage*)image
    didFinishSavingWithError:(NSError*)error
                 contextInfo:(void*)contextInfo {
  // Was there an error?
  if (error) {
    // Saving photo failed, likely due to a permissions issue.
    // This code may be execute outside of the main thread. Make sure to display
    // the error on the main thread.
    [self displayImageErrorAlertWithSettingsOnMainQueue];
  } else {
    // TODO(crbug.com/41362123): Provide a way for the user to easily reach the
    // photos app.
  }
}

@end
