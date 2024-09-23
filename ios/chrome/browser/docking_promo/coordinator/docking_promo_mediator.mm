// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/docking_promo/coordinator/docking_promo_mediator.h"

#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/docking_promo/model/utils.h"
#import "ios/chrome/browser/docking_promo/ui/docking_promo_consumer.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
NSString* const kDockingPromoAnimation = @"docking_promo";
}  // namespace

@interface DockingPromoMediator ()

// The PromosManager is used to register the Docking Promo.
@property(nonatomic, assign) PromosManager* promosManager;

// The time since last foregrounding.
@property(nonatomic, assign) base::TimeDelta timeSinceLastForeground;

@end

@implementation DockingPromoMediator

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
              timeSinceLastForeground:(base::TimeDelta)timeSinceLastForeground {
  if ((self = [super init])) {
    _promosManager = promosManager;
    _timeSinceLastForeground = timeSinceLastForeground;
  }
  return self;
}

- (BOOL)canShowDockingPromo {
  PrefService* localPrefService = GetApplicationContext()->GetLocalState();

  BOOL promoEligibilityMet =
      localPrefService->GetBoolean(prefs::kIosDockingPromoEligibilityMet);

  // Determines if a user should see the Docking Promo. For users in the
  // "eligible-only-users" group, eligibility is locked-in: if they were ever
  // eligible, they remain eligible. This is made possible via the pref
  // `kIosDockingPromoEligibilityMet`, and simplifies metrics analysis.
  //
  // NOTE: It's important to check Docking Promo eligibility before the Finch
  // check. This prevents ineligible users from ever being included in the
  // `kIOSDockingPromoForEligibleUsersOnly` experiment.
  if (promoEligibilityMet && IsDockingPromoForEligibleUsersOnlyEnabled()) {
    return YES;
  }

  return CanShowDockingPromo(_timeSinceLastForeground);
}

- (void)configureConsumer {
  [self setTextAndImage];
}

- (void)registerPromoWithPromosManager {
  if (!self.promosManager) {
    return;
  }

  self.promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::DockingPromoRemindMeLater);
}

#pragma mark - Private

- (void)setTextAndImage {
  CHECK(self.consumer);

  NSString* titleString = l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_TITLE);
  NSString* primaryActionString =
      l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_PRIMARY_BUTTON_TITLE);
  NSString* secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_DOCKING_PROMO_SECONDARY_BUTTON_TITLE);
  NSString* animationName = kDockingPromoAnimation;

  [self.consumer setTitleString:titleString
            primaryActionString:primaryActionString
          secondaryActionString:secondaryActionString
                  animationName:animationName];
}

@end
