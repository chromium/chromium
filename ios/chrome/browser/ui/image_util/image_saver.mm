// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/image_util/image_saver.h"

#import <Photos/Photos.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/web/image_fetch_tab_helper.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ImageSaver ()
// Base view controller for the alerts.
@property(nonatomic, weak) UIViewController* baseViewController;
// Alert coordinator to give feedback to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
@end

@implementation ImageSaver

@synthesize alertCoordinator = _alertCoordinator;
@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
  }
  return self;
}

- (void)saveImageAtURL:(const GURL&)url
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState {
  ImageFetchTabHelper* tabHelper = ImageFetchTabHelper::FromWebState(webState);
  DCHECK(tabHelper);

  __weak ImageSaver* weakSelf = self;
  tabHelper->GetImageData(url, referrer, ^(NSData* data) {
    ImageSaver* strongSelf = weakSelf;
    if (!strongSelf)
      return;

    if (data.length == 0) {
      [strongSelf displayPrivacyErrorAlertOnMainQueue:
                      l10n_util::GetNSString(
                          IDS_IOS_SAVE_IMAGE_NO_INTERNET_CONNECTION)];
      return;
    }

    NSString* extension = GetImageExtensionFromData(data);
    NSString* fileExtension =
        [@"." stringByAppendingString:extension ? extension : @"png"];

    [strongSelf managePermissionAndSaveImage:data
                           withFileExtension:fileExtension];
  });
}

// Saves the image or display error message, based on privacy settings.
- (void)managePermissionAndSaveImage:(NSData*)data
                   withFileExtension:(NSString*)fileExtension {
  switch ([PHPhotoLibrary authorizationStatus]) {
    // User was never asked for permission to access photos.
    case PHAuthorizationStatusNotDetermined: {
      [PHPhotoLibrary requestAuthorization:^(PHAuthorizationStatus status) {
        // Call -saveImage again to check if chrome needs to display an error or
        // saves the image.
        if (status != PHAuthorizationStatusNotDetermined)
          [self managePermissionAndSaveImage:data
                           withFileExtension:fileExtension];
      }];
      break;
    }

    // The application doesn't have permission to access photo and the user
    // cannot grant it.
    case PHAuthorizationStatusRestricted:
      [self displayPrivacyErrorAlertOnMainQueue:
                l10n_util::GetNSString(
                    IDS_IOS_SAVE_IMAGE_RESTRICTED_PRIVACY_ALERT_MESSAGE)];
      break;

    // The application doesn't have permission to access photo and the user
    // can grant it.
    case PHAuthorizationStatusDenied:
      [self displayImageErrorAlertWithSettingsOnMainQueue];
      break;

    // The application has permission to access the photos.
    default:
      __weak ImageSaver* weakSelf = self;
      [self saveImage:data
          withFileExtension:fileExtension
                 completion:^(BOOL success, NSError* error) {
                   [weakSelf finishSavingImageWithError:error];
                 }];
      break;
  }
}

// Saves the image. In order to keep the metadata of the image, the image is
// saved as a temporary file on disk then saved in photos. Saving will happen
// on a background sequence and the completion block will be invoked on that
// sequence.
- (void)saveImage:(NSData*)data
    withFileExtension:(NSString*)fileExtension
           completion:(void (^)(BOOL, NSError*))completion {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(^{
        base::ScopedBlockingCall scoped_blocking_call(
            FROM_HERE, base::BlockingType::MAY_BLOCK);

        NSString* fileName = [[[NSProcessInfo processInfo] globallyUniqueString]
            stringByAppendingString:fileExtension];
        NSURL* fileURL = [NSURL
            fileURLWithPath:[NSTemporaryDirectory()
                                stringByAppendingPathComponent:fileName]];
        NSError* error = nil;
        [data writeToURL:fileURL options:NSDataWritingAtomic error:&error];
        if (error) {
          if (completion)
            completion(NO, error);
          return;
        }

        [[PHPhotoLibrary sharedPhotoLibrary]
            performChanges:^{
              [PHAssetChangeRequest
                  creationRequestForAssetFromImageAtFileURL:fileURL];
            }
            completionHandler:^(BOOL success, NSError* error) {
              base::PostTask(
                  FROM_HERE,
                  {base::ThreadPool(), base::MayBlock(),
                   base::TaskPriority::BEST_EFFORT,
                   base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                  base::BindOnce(^{
                    base::ScopedBlockingCall scoped_blocking_call(
                        FROM_HERE, base::BlockingType::MAY_BLOCK);
                    if (completion)
                      completion(success, error);

                    // Cleanup the temporary file.
                    NSError* deleteFileError = nil;
                    [[NSFileManager defaultManager]
                        removeItemAtURL:fileURL
                                  error:&deleteFileError];
                  }));
            }];
      }));
}

// Called when Chrome has been denied access to the photos or videos and the
// user can change it.
// Shows a privacy alert on the main queue, allowing the user to go to Chrome's
// settings. Dismiss previous alert if it has not been dismissed yet.
- (void)displayImageErrorAlertWithSettingsOnMainQueue {
  NSURL* settingURL = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
  BOOL canGoToSetting =
      [[UIApplication sharedApplication] canOpenURL:settingURL];
  if (canGoToSetting) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self displayImageErrorAlertWithSettings:settingURL];
    });
  } else {
    [self displayPrivacyErrorAlertOnMainQueue:
              l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_MESSAGE)];
  }
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
// Shows a privacy alert on the main queue, with errorContent as the message.
// Dismisses previous alert if it has not been dismissed yet.
- (void)displayPrivacyErrorAlertOnMainQueue:(NSString*)errorContent {
  dispatch_async(dispatch_get_main_queue(), ^{
    NSString* title =
        l10n_util::GetNSString(IDS_IOS_SAVE_IMAGE_PRIVACY_ALERT_TITLE);
    // Dismiss current alert.
    [self.alertCoordinator stop];

    self.alertCoordinator = [[AlertCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                             title:title
                           message:errorContent];
    [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                                     action:nil
                                      style:UIAlertActionStyleDefault];
    [self.alertCoordinator start];
  });
}

// This callback is triggered when the image is effectively saved onto the photo
// album, or if the save failed for some reason.
- (void)finishSavingImageWithError:(NSError*)error {
  // Was there an error?
  if (error) {
    // Saving photo failed even though user has granted access to Photos.
    // Display the error information from the NSError object for user.
    NSString* errorMessage = [NSString
        stringWithFormat:@"%@ (%@ %" PRIdNS ")", [error localizedDescription],
                         [error domain], [error code]];
    // This code may be execute outside of the main thread. Make sure to display
    // the error on the main thread.
    [self displayPrivacyErrorAlertOnMainQueue:errorMessage];
  } else {
    // TODO(crbug.com/797277): Provide a way for the user to easily reach the
    // photos app.
  }
}

@end
