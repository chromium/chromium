// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/coordinator/default_browser_mediator.h"

#import "components/ntp_tiles/pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/ui/default_browser_commands.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/default_browser/ui/default_browser_config.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

@interface DefaultBrowserMediator () <DefaultBrowserCommands,
                                      PrefObserverDelegate>

@end

@implementation DefaultBrowserMediator {
  // The profile Pref service.
  raw_ptr<PrefService> _profilePrefService;

  // Registrar for user Pref changes notifications.
  PrefChangeRegistrar _profilePrefChangeRegistrar;

  // Bridge to listen to Pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
}

- (instancetype)initWithProfilePrefService:(PrefService*)profilePrefService {
  if ((self = [super init])) {
    CHECK(profilePrefService);
    _profilePrefService = profilePrefService;
    self.config = [[DefaultBrowserConfig alloc] init];
    self.config.commandHandler = self;

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
  _profilePrefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _profilePrefService = nil;
}

- (void)removeModuleWithCompletion:(ProceduralBlock)completion {
  [self.delegate removeDefaultBrowserPromoModuleWithCompletion:completion];
}

#pragma mark - DefaultBrowserCommands

- (void)didTapDefaultBrowserPromo {
  [self.presentationAudience didTapDefaultBrowserPromo];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  CHECK(_profilePrefService);
  CHECK_EQ(preferenceName, ntp_tiles::prefs::kTipsHomeModuleEnabled);
  if (!_profilePrefService->GetBoolean(
          ntp_tiles::prefs::kTipsHomeModuleEnabled)) {
    [self.delegate removeDefaultBrowserPromoModuleWithCompletion:nil];
  }
}

@end
