// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_mediator.h"

#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_member.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_opt_in_status.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/settings/google_services/parcel_tracking_settings_model_consumer.h"

@interface ParcelTrackingSettingsMediator () <PrefObserverDelegate>
@end

@implementation ParcelTrackingSettingsMediator {
  IntegerPrefMember _optInStatus;
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
}

- (instancetype)initWithPrefs:(PrefService*)prefs {
  self = [super init];
  if (self) {
    _optInStatus.Init(prefs::kIosParcelTrackingOptInStatus, prefs);

    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(prefs);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIosParcelTrackingOptInStatus, _prefChangeRegistrar.get());
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.reset();
  _prefChangeRegistrar = nullptr;
  _prefObserverBridge.reset();
  _prefObserverBridge = nullptr;
  _optInStatus.Destroy();
}

- (void)setConsumer:(id<ParcelTrackingSettingsModelConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [self.consumer updateCheckedState:static_cast<IOSParcelTrackingOptInStatus>(
                                        _optInStatus.GetValue())];
}

#pragma mark - ParcelTrackingSettingsModelDelegate

- (void)tableViewDidSelectStatus:(IOSParcelTrackingOptInStatus)status {
  [self updateSetting:status];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kIosParcelTrackingOptInStatus) {
    [self.consumer updateCheckedState:static_cast<IOSParcelTrackingOptInStatus>(
                                          _optInStatus.GetValue())];
  }
}

#pragma mark - Helpers

// Updates the pref value with `newSetting`.
- (void)updateSetting:(IOSParcelTrackingOptInStatus)newSetting {
  _optInStatus.SetValue(static_cast<int>(newSetting));
}



@end
