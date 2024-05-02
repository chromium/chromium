// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_promo_manager.h"

#import "base/notreached.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/default_promo/generic/default_browser_generic_promo_commands.h"
#import "ios/chrome/browser/ui/default_promo/generic/default_browser_generic_promo_coordinator.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"

@interface DefaultBrowserPromoManager () <DefaultBrowserGenericPromoCommands>

// Default browser promo command handler.
@property(nonatomic, readonly) id<DefaultBrowserGenericPromoCommands>
    defaultBrowserPromoHandler;

// Coordinator for the video default browser promo.
@property(nonatomic, strong) DefaultBrowserGenericPromoCoordinator*
    defaultBrowserGenericPromoCoordinator;

// Feature engagement tracker reference.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

@end

@implementation DefaultBrowserPromoManager

#pragma mark - ChromeCoordinator

- (void)start {
  self.tracker = feature_engagement::TrackerFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  [self showVideoPromo];
}

- (void)stop {
  [self.defaultBrowserGenericPromoCoordinator stop];
  if (self.promoWasFromRemindMeLater && self.tracker) {
    self.tracker->Dismissed(
        feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature);
  }
  self.defaultBrowserGenericPromoCoordinator = nil;

  [self.promosUIHandler promoWasDismissed];
  self.promosUIHandler = nil;

  [super stop];
}

#pragma mark - DefaultBrowserPromoCommands

- (void)hidePromo {
  id<DefaultBrowserGenericPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DefaultBrowserGenericPromoCommands);
  [handler hidePromo];
}

#pragma mark - public

+ (void)showPromoForTesting:(DefaultPromoType)promoType {
}

#pragma mark - private

- (void)showVideoPromo {
  self.defaultBrowserGenericPromoCoordinator =
      [[DefaultBrowserGenericPromoCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser];
  self.defaultBrowserGenericPromoCoordinator.handler = self;
  BOOL hasRemindMeLater =
      base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature) &&
      !self.promoWasFromRemindMeLater;
  self.defaultBrowserGenericPromoCoordinator.hasRemindMeLater =
      hasRemindMeLater;
  [self.defaultBrowserGenericPromoCoordinator start];

  // Used for testing only.
  [DefaultBrowserPromoManager showPromoForTesting:DefaultPromoTypeVideo];
}

@end
