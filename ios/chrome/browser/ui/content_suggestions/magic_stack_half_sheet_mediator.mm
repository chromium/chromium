// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_utils.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack_half_sheet_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"

@interface MagicStackHalfSheetMediator () <BooleanObserver>
@end

@implementation MagicStackHalfSheetMediator {
  raw_ptr<PrefService> _localState;
  raw_ptr<PrefService> _profilePrefs;

  PrefBackedBoolean* _mostVisitedSitesEnabled;
  PrefBackedBoolean* _setUpListDisabled;
  PrefBackedBoolean* _safetyCheckDisabled;
  PrefBackedBoolean* _tabResumptionDisabled;
  PrefBackedBoolean* _parcelTrackingDisabled;
}

- (instancetype)initWithLocalState:(PrefService*)localState
                profilePrefService:(PrefService*)profilePrefs {
  if ((self = [super init])) {
    CHECK(localState);
    CHECK(profilePrefs);
    _localState = localState;
    _profilePrefs = profilePrefs;
    if (ShouldPutMostVisitedSitesInMagicStack(
            FeedActivityBucketForPrefs(_profilePrefs))) {
      _mostVisitedSitesEnabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_profilePrefs
                     prefName:prefs::kHomeCustomizationMostVisitedEnabled];
      [_mostVisitedSitesEnabled setObserver:self];
    }
    if (set_up_list_utils::IsSetUpListActive(_localState, nil /*user_prefs*/,
                                             false)) {
      _setUpListDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_localState
                     prefName:set_up_list_prefs::kDisabled];
      [_setUpListDisabled setObserver:self];
    }
    if (IsSafetyCheckMagicStackEnabled()) {
      _safetyCheckDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_profilePrefs
                     prefName:safety_check_prefs::
                                  kSafetyCheckInMagicStackDisabledPref];
      [_safetyCheckDisabled setObserver:self];
    }
    if (IsTabResumptionEnabled()) {
      _tabResumptionDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_profilePrefs
                     prefName:tab_resumption_prefs::kTabResumptionDisabledPref];
      [_tabResumptionDisabled setObserver:self];
    }
    if (IsIOSParcelTrackingEnabled()) {
      _parcelTrackingDisabled = [[PrefBackedBoolean alloc]
          initWithPrefService:_localState
                     prefName:kParcelTrackingDisabled];
      [_parcelTrackingDisabled setObserver:self];
    }
  }
  return self;
}

- (void)disconnect {
  _localState = nil;
  _profilePrefs = nil;
  if (_mostVisitedSitesEnabled) {
    [_mostVisitedSitesEnabled setObserver:nil];
    _mostVisitedSitesEnabled = nil;
  }
  if (_setUpListDisabled) {
    [_setUpListDisabled setObserver:nil];
    _setUpListDisabled = nil;
  }
  if (_safetyCheckDisabled) {
    [_safetyCheckDisabled setObserver:nil];
    _safetyCheckDisabled = nil;
  }
  if (_tabResumptionDisabled) {
    [_tabResumptionDisabled setObserver:nil];
    _tabResumptionDisabled = nil;
  }
  if (_parcelTrackingDisabled) {
    [_parcelTrackingDisabled setObserver:nil];
    _parcelTrackingDisabled = nil;
  }
}

- (void)setConsumer:(id<MagicStackHalfSheetConsumer>)consumer {
  _consumer = consumer;
  if (_mostVisitedSitesEnabled) {
    [self.consumer showMostVisitedSitesToggle:YES];
    [self.consumer setMostVisitedSitesEnabled:_mostVisitedSitesEnabled.value];
  }
  if (_setUpListDisabled) {
    [self.consumer showSetUpList:YES];
    [self.consumer setSetUpListDisabled:_setUpListDisabled.value];
  }
  if (_safetyCheckDisabled) {
    [self.consumer setSafetyCheckDisabled:_safetyCheckDisabled.value];
  }
  if (_tabResumptionDisabled) {
    [self.consumer setTabResumptionDisabled:_tabResumptionDisabled.value];
  }
  if (_parcelTrackingDisabled) {
    [self.consumer setParcelTrackingDisabled:_parcelTrackingDisabled.value];
  }
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _mostVisitedSitesEnabled) {
    [self.consumer setMostVisitedSitesEnabled:_mostVisitedSitesEnabled.value];
  } else if (observableBoolean == _setUpListDisabled) {
    [self.consumer setSetUpListDisabled:_setUpListDisabled.value];
  } else if (observableBoolean == _safetyCheckDisabled) {
    [self.consumer setSafetyCheckDisabled:_safetyCheckDisabled.value];
  } else if (observableBoolean == _tabResumptionDisabled) {
    [self.consumer setTabResumptionDisabled:_tabResumptionDisabled.value];
  } else if (observableBoolean == _parcelTrackingDisabled) {
    [self.consumer setParcelTrackingDisabled:_parcelTrackingDisabled.value];
  }
}

#pragma mark - MagicStackHalfSheetDelegate

- (void)mostVisitedSitesEnabledChanged:(BOOL)mostVisitedSitesEnabled {
  CHECK(_mostVisitedSitesEnabled);
  [_mostVisitedSitesEnabled setValue:mostVisitedSitesEnabled];
}

- (void)setUpListEnabledChanged:(BOOL)setUpListEnabled {
  [_setUpListDisabled setValue:!setUpListEnabled];
}

- (void)safetyCheckEnabledChanged:(BOOL)safetyCheckEnabled {
  [_safetyCheckDisabled setValue:!safetyCheckEnabled];
}

- (void)tabResumptionEnabledChanged:(BOOL)tabResumptionEnabled {
  [_tabResumptionDisabled setValue:!tabResumptionEnabled];
}

- (void)parcelTrackingEnabledChanged:(BOOL)parcelTrackingEnabled {
  [_parcelTrackingDisabled setValue:!parcelTrackingEnabled];
}

@end
