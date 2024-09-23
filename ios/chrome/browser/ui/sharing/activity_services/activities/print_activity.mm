// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/activities/print_activity.h"

#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_image_data.h"
#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kPrintActivityType = @"com.google.chrome.printActivity";

}  // namespace

@interface PrintActivity ()
// The data object targeted by this activity if it comes from a tab.
@property(nonatomic, strong, readonly) ShareToData* webData;
// The data object targeted by this activity if it comes from an image.
@property(nonatomic, strong, readonly) ShareImageData* imageData;
// The handler to be invoked when the activity is performed.
@property(nonatomic, weak, readonly) id<BrowserCoordinatorCommands> handler;
// The base VC to present print preview.
@property(nonatomic, weak) UIViewController* baseViewController;

@end

@implementation PrintActivity

- (instancetype)initWithData:(ShareToData*)webData
                     handler:(id<BrowserCoordinatorCommands>)handler
          baseViewController:(UIViewController*)baseViewController {
  if ((self = [super init])) {
    _webData = webData;
    _handler = handler;
    _baseViewController = baseViewController;
  }
  return self;
}

- (instancetype)initWithImageData:(ShareImageData*)imageData
                          handler:(id<BrowserCoordinatorCommands>)handler
               baseViewController:(UIViewController*)baseViewController {
  if ((self = [super init])) {
    _imageData = imageData;
    _handler = handler;
    _baseViewController = baseViewController;
  }
  return self;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kPrintActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_IOS_SHARE_MENU_PRINT_ACTION);
}

- (UIImage*)activityImage {
  return DefaultSymbolWithPointSize(kPrinterSymbol, kSymbolActionPointSize);
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  if (self.webData) {
    return self.webData.isPagePrintable;
  } else {
    return self.imageData.image != nil;
  }
}

- (void)prepareWithActivityItems:(NSArray*)activityItems {
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  // UIActivityViewController and UIPrintInteractionController are UIKit VCs for
  // which presentation is not fully controlable.
  // If UIActivityViewController is visible when UIPrintInteractionController
  // is presented, the print VC will be dismissed when the activity VC is
  // dismissed (even if UIPrintInteractionControllerDelegate provides another
  // parent VC.
  // To avoid this issue, dismiss first and present print after.
  [self activityDidFinish:YES];
  if (self.webData) {
    [self.handler printTabWithBaseViewController:self.baseViewController];
  } else {
    [self.handler printImage:self.imageData.image
                       title:self.imageData.title
          baseViewController:self.baseViewController];
  }
}

@end
