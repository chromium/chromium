// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator_implementation.h"
#include "ios/chrome/grit/ios_google_chrome_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DefaultBrowserPromoNonModalCoordinator ()
// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// YES if the Infobar has been Accepted.
@property(nonatomic, assign) BOOL infobarAccepted;

@end

@implementation DefaultBrowserPromoNonModalCoordinator

// Synthesize because readonly property from superclass is changed to readwrite.
@synthesize bannerViewController = _bannerViewController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithInfoBarDelegate:nil
                           badgeSupport:YES
                                   type:InfobarType::kInfobarTypeConfirm];
  if (self) {
    self.baseViewController = viewController;
    self.browser = browser;
  }
  return self;
}

- (void)start {
  if (!self.started) {
    self.started = YES;
    self.bannerViewController =
        [[InfobarBannerViewController alloc] initWithDelegate:self
                                                presentsModal:NO
                                                         type:self.infobarType];
    [self.bannerViewController
        setTitleText:l10n_util::GetNSString(
                         IDS_IOS_DEFAULT_BROWSER_NON_MODAL_TITLE)];
    [self.bannerViewController
        setSubtitleText:l10n_util::GetNSString(
                            IDS_IOS_DEFAULT_BROWSER_NON_MODAL_DESCRIPTION)];
    [self.bannerViewController
        setButtonText:l10n_util::GetNSString(
                          IDS_IOS_DEFAULT_NON_MODAL_PRIMARY_BUTTON_TEXT)];
    // TODO(crbug.com/1198995): Add image when that is available from design.
    [self.bannerViewController setPresentsModal:NO];
  }
}

#pragma mark - InfobarCoordinatorImplementation

- (BOOL)configureModalViewController {
  return NO;
}

- (BOOL)isInfobarAccepted {
  return self.infobarAccepted;
}

- (BOOL)infobarBannerActionWillPresentModal {
  return NO;
}

- (void)infobarBannerWasPresented {
  // No-op.
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  // No-op. Should never happen as the non-modal promo should never have a
  // modal.
}

- (void)dismissBannerIfReady {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

- (BOOL)infobarActionInProgress {
  return NO;
}

- (void)performInfobarAction {
  self.infobarAccepted = YES;
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  // No-op.
}

- (void)infobarWasDismissed {
  self.bannerViewController = nil;
  id<DefaultBrowserPromoNonModalCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         DefaultBrowserPromoNonModalCommands);
  [handler defaultBrowserNonModalPromoWasDismissed];
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  // The non-modal promo should never have a modal.
  NOTREACHED();
  return 0;
}

@end
