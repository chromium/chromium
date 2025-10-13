// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/coordinator/app_bundle_promo_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/app_store_bundle/model/app_store_bundle_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/app_bundle_promo/ui/app_bundle_promo_config.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_view_controller_audience.h"

@interface AppBundlePromoMediator () <AppBundlePromoAudience,
                                      PrefObserverDelegate>

@end

@implementation AppBundlePromoMediator {
  // The App Store Bundle service.
  raw_ptr<AppStoreBundleService> _appStoreBundleService;

  // The profile Pref service.
  raw_ptr<PrefService> _profilePrefService;

  // Registrar for user Pref changes notifications.
  PrefChangeRegistrar _profilePrefChangeRegistrar;

  // Bridge to listen to Pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
}

- (instancetype)initWithAppStoreBundleService:
                    (AppStoreBundleService*)appStoreBundleService
                           profilePrefService:(PrefService*)profilePrefService {
  if ((self = [super init])) {
    CHECK(appStoreBundleService);
    CHECK(profilePrefService);
    _appStoreBundleService = appStoreBundleService;
    _profilePrefService = profilePrefService;
    self.config = [[AppBundlePromoConfig alloc] init];
    self.config.audience = self;

    if (!_prefObserverBridge) {
      _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);

      _profilePrefChangeRegistrar.Init(profilePrefService);

      _prefObserverBridge->ObserveChangesForPreference(
          ntp_tiles::prefs::kTipsHomeModuleEnabled,
          &_profilePrefChangeRegistrar);
    }
  }
  return self;
}

- (void)disconnect {
  self.config = nil;
  _appStoreBundleService = nil;
  _profilePrefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _profilePrefService = nil;
}

- (void)removeModuleWithCompletion:(ProceduralBlock)completion {
  [self.delegate removeAppBundlePromoModuleWithCompletion:completion];
}

- (void)didSelectAppBundlePromo {
  [self.delegate logMagicStackEngagementForType:self.config.type];
  [self.presentationAudience didSelectAppBundlePromo];
}

- (void)presentAppStoreBundlePage:(UIViewController*)baseViewController
                   withCompletion:(ProceduralBlock)completion {
  CHECK(_appStoreBundleService);
  _appStoreBundleService->PresentAppStoreBundlePromo(baseViewController,
                                                     completion);
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  CHECK(_profilePrefService);
  CHECK_EQ(preferenceName, ntp_tiles::prefs::kTipsHomeModuleEnabled);
  if (!_profilePrefService->GetBoolean(
          ntp_tiles::prefs::kTipsHomeModuleEnabled)) {
    [self.delegate removeAppBundlePromoModuleWithCompletion:nil];
  }
}

@end
