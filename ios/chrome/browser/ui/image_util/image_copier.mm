// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "image_copier.h"

#include <MobileCoreServices/MobileCoreServices.h>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/web/image_fetch_tab_helper.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Name of the UMA ContextMenu.iOS.CopyImage histogram.
const char kUmaContextMenuCopyImage[] = "ContextMenu.iOS.CopyImage";
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Enum for the ContextMenu.iOS.CopyImage UMA histogram to report
// the results of Copy Image.
enum class ContextMenuCopyImage {
  // Copy Image is called.
  kInvoked = 0,
  // Image data is fetched.
  kImageFetched = 1,
  // Image data is fetched, and Copy Image is not canceled by user.
  kTryCopyImage = 2,
  // Fetched image data is valid and copied to pasteboard.
  kImageCopied = 3,
  // Fetching image data takes too long, a waiting alert popped up.
  kAlertPopUp = 4,
  // Copy Image is canceled by user from the alert.
  kCanceled = 5,
  kMaxValue = kCanceled,
};
// Time Period between "Copy Image" is clicked and "Copying..." alert is
// launched.
const int kAlertDelayInMs = 300;
// A speical id indicates that last copy is finished or canceled and next
// copy has not started.
const int kNoActiveCopy = 0;
}

@interface ImageCopier ()
// Base view controller for the alerts.
@property(nonatomic, weak) UIViewController* baseViewController;
// Alert coordinator to give feedback to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// A counter which generates one ID for each call on
// CopyImageAtURL:referrer:webState.
@property(nonatomic) int idGenerator;
// ID of current active copy. A copy is active after
// CopyImageAtURL:referrer:webState is called, and before user cancels the
// copy or the copy finishes.
@property(nonatomic) int activeID;

@end

@implementation ImageCopier

@synthesize alertCoordinator = _alertCoordinator;
@synthesize baseViewController = _baseViewController;
@synthesize idGenerator = _idGenerator;
@synthesize activeID = _activeID;

- (instancetype)initWithBaseViewController:
    (UIViewController*)baseViewController {
  self = [super init];
  if (self) {
    self.idGenerator = 1;
    self.activeID = kNoActiveCopy;
    self.baseViewController = baseViewController;
  }
  return self;
}

- (void)copyImageAtURL:(const GURL&)url
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState {
  __weak ImageCopier* weakSelf = self;

  // |idGenerator| is initiated to 1 and incremented by 2, so it will always be
  // odd number and won't collides with |kNoActiveCopy| or nil.activeID, which
  // are 0s.
  self.idGenerator += 2;
  self.activeID = self.idGenerator;
  // This var is to be captured by blocks in ImageFetchTabHelper::GetImageData
  // and web::WebThread::PostDelayedTask. When a block is invoked, it uses the
  // captured |callbackID| to check if the copy from where it's started is still
  // alive or has been finished/canceled.
  int callbackID = self.idGenerator;

  ImageFetchTabHelper* tabHelper = ImageFetchTabHelper::FromWebState(webState);
  DCHECK(tabHelper);
  NSString* urlStr = base::SysUTF8ToNSString(url.spec());
  tabHelper->GetImageData(url, referrer, ^(NSData* data) {
    // Check that the copy has not been canceled.
    if (callbackID == weakSelf.activeID) {
      [weakSelf.alertCoordinator stop];
      weakSelf.activeID = kNoActiveCopy;

      // Copy image url and data to pasteboard.
      NSMutableDictionary* item =
          [NSMutableDictionary dictionaryWithCapacity:3];
      [item setValue:urlStr forKey:(__bridge NSString*)kUTTypeText];
      [item setValue:[NSURL URLWithString:urlStr]
              forKey:(__bridge NSString*)kUTTypeURL];
      NSString* uti = GetImageUTIFromData(data);
      if (uti) {
        [item setValue:data forKey:uti];
        [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kImageCopied];
      }
      UIPasteboard.generalPasteboard.items =
          [NSMutableArray arrayWithObject:item];
      [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kTryCopyImage];
    }
    [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kImageFetched];
  });

  // Dismiss current alert.
  [self.alertCoordinator stop];
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                           title:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_CONTENT_COPYIMAGE_ALERT_COPYING)
                         message:nil];
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)
                action:^() {
                  // Cancels current copy and closes the alert.
                  weakSelf.activeID = kNoActiveCopy;
                  [weakSelf.alertCoordinator stop];
                  [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kCanceled];
                }
                 style:UIAlertActionStyleCancel];

  // Delays launching alert by |kAlertDelayInMs|.
  base::PostDelayedTask(
      FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
        // Checks that the copy has not finished yet.
        if (callbackID == weakSelf.activeID) {
          [weakSelf.alertCoordinator start];
          [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kAlertPopUp];
        }
      }),
      base::TimeDelta::FromMilliseconds(kAlertDelayInMs));

  [self recordCopyImageUMA:ContextMenuCopyImage::kInvoked];
}

- (void)recordCopyImageUMA:(ContextMenuCopyImage)UMAEnum {
  UMA_HISTOGRAM_ENUMERATION(kUmaContextMenuCopyImage, UMAEnum);
}

@end
