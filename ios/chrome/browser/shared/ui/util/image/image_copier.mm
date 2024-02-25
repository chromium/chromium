// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "image_copier.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/functional/bind.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Enum for the Mobile.ContextMenu.CopyImage UMA histogram to report
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
  // The URL of the image is copied.
  kURLCopied = 6,
  kMaxValue = kURLCopied,
};
// Time Period between "Copy Image" is clicked and "Copying..." alert is
// launched.
const int kAlertDelayInMs = 300;
// A speical id indicates that last copy is finished or canceled and next
// copy has not started.
const int kNoActiveCopy = 0;
}  // namespace

@interface ImageCopier ()
// The browser.
@property(nonatomic, assign) Browser* browser;
// Alert coordinator to give feedback to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
// A counter which generates one ID for each call on
// CopyImageAtURL:referrer:webState.
@property(nonatomic, assign) int idGenerator;
// ID of current active copy. A copy is active after
// CopyImageAtURL:referrer:webState is called, and before user cancels the
// copy or the copy finishes.
@property(nonatomic, assign) int activeID;

@end

@implementation ImageCopier

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    _idGenerator = 1;
    _activeID = kNoActiveCopy;
    _browser = browser;
  }
  return self;
}

- (void)stop {
  self.browser = nullptr;
  [self stopAlertCoordinator];
}

- (void)copyImageAtURL:(const GURL&)url
              referrer:(const web::Referrer&)referrer
              webState:(web::WebState*)webState
    baseViewController:(UIViewController*)baseViewController {
  __weak ImageCopier* weakSelf = self;

  // `idGenerator` is initiated to 1 and incremented by 2, so it will always be
  // odd number and won't collides with `kNoActiveCopy` or nil.activeID, which
  // are 0s.
  self.idGenerator += 2;
  self.activeID = self.idGenerator;
  // This var is to be captured by blocks in ImageFetchTabHelper::GetImageData
  // and web::WebThread::PostDelayedTask. When a block is invoked, it uses the
  // captured `callbackID` to check if the copy from where it's started is still
  // alive or has been finished/canceled.
  int callbackID = self.idGenerator;

  ImageFetchTabHelper* tabHelper = ImageFetchTabHelper::FromWebState(webState);
  DCHECK(tabHelper);
  NSString* urlStr = base::SysUTF8ToNSString(url.spec());
  tabHelper->GetImageData(url, referrer, ^(NSData* data) {
    // Check that the copy has not been canceled.
    if (callbackID == weakSelf.activeID) {
      [weakSelf stopAlertCoordinator];
      weakSelf.activeID = kNoActiveCopy;

      ImageCopyResult result =
          StoreImageInPasteboard(data, [NSURL URLWithString:urlStr]);
      switch (result) {
        case ImageCopyResult::kImage:
          [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kImageCopied];
          break;
        case ImageCopyResult::kURL:
          [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kURLCopied];
          break;
      }
      [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kTryCopyImage];
    }
    [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kImageFetched];
  });

  // Dismiss current alert.
  [self stopAlertCoordinator];
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:self.browser
                           title:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_CONTENT_COPYIMAGE_ALERT_COPYING)
                         message:nil];
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSStringWithFixup(IDS_CANCEL)
                action:^() {
                  // Cancels current copy and closes the alert.
                  weakSelf.activeID = kNoActiveCopy;
                  [weakSelf stopAlertCoordinator];
                  [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kCanceled];
                }
                 style:UIAlertActionStyleCancel];

  // Delays launching alert by `kAlertDelayInMs`.
  web::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        // Checks that the copy has not finished yet.
        if (callbackID == weakSelf.activeID) {
          [weakSelf.alertCoordinator start];
          [weakSelf recordCopyImageUMA:ContextMenuCopyImage::kAlertPopUp];
        }
      }),
      base::Milliseconds(kAlertDelayInMs));

  [self recordCopyImageUMA:ContextMenuCopyImage::kInvoked];
}

#pragma mark - Private

// Records in UMA the Copy Image with `UMAEnum`.
- (void)recordCopyImageUMA:(ContextMenuCopyImage)UMAEnum {
  UMA_HISTOGRAM_ENUMERATION("Mobile.ContextMenu.CopyImage", UMAEnum);
}

// Stops the alert coordinator.
- (void)stopAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

@end
