// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_promo_manager.h"

#import "base/notreached.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_promo_commands.h"
#import "ios/chrome/browser/ui/default_promo/tailored_promo_coordinator.h"
#import "ios/chrome/browser/ui/default_promo/video_default_browser_promo_coordinator.h"
#import "ios/chrome/browser/ui/policy/user_policy_util.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_ui_handler.h"

@interface DefaultBrowserPromoManager () <DefaultBrowserPromoCommands>

// Default browser promo command handler.
@property(nonatomic, readonly) id<DefaultBrowserPromoCommands>
    defaultBrowserPromoHandler;

// Coordinator for the video default browser promo.
@property(nonatomic, strong)
    VideoDefaultBrowserPromoCoordinator* videoDefaultPromoCoordinator;

// Feature engagement tracker reference.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

@end

@implementation DefaultBrowserPromoManager

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  PrefService* prefService = browserState->GetPrefs();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  self.tracker = feature_engagement::TrackerFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  policy::UserCloudPolicyManager* user_policy_manager =
      browserState->GetUserCloudPolicyManager();

  if (IsUserPolicyNotificationNeeded(authService, prefService,
                                     user_policy_manager)) {
    // Showing the User Policy notification has priority over showing the
    // default browser promo. Both dialogs are competing for the same time slot
    // which is after the browser startup and the browser UI is initialized.
    [self hidePromo];
    return;
  }

  [self showVideoPromo];
}

- (void)stop {
  [self.videoDefaultPromoCoordinator stop];
  if (self.promoWasFromRemindMeLater && self.tracker) {
    self.tracker->Dismissed(
        feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature);
  }
  self.videoDefaultPromoCoordinator = nil;

  [self.promosUIHandler promoWasDismissed];
  self.promosUIHandler = nil;

  [super stop];
}

#pragma mark - DefaultBrowserPromoCommands

- (void)hidePromo {
  id<DefaultBrowserPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), DefaultBrowserPromoCommands);
  [handler hidePromo];
}

#pragma mark - public

+ (void)showPromoForTesting:(DefaultPromoType)promoType {
}

#pragma mark - private

- (void)showVideoPromo {
  self.videoDefaultPromoCoordinator =
      [[VideoDefaultBrowserPromoCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser];
  self.videoDefaultPromoCoordinator.handler = self;
  BOOL showRemindMeLater =
      base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSPromoDefaultBrowserReminderFeature) &&
      !self.promoWasFromRemindMeLater;
  self.videoDefaultPromoCoordinator.showRemindMeLater = showRemindMeLater;
  [self.videoDefaultPromoCoordinator start];

  // Used for testing only.
  [DefaultBrowserPromoManager showPromoForTesting:DefaultPromoTypeVideo];
}

@end
