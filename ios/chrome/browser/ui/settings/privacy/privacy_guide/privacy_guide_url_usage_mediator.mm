// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_mediator.h"

#import "base/check.h"
#import "components/prefs/pref_service.h"
#import "base/memory/raw_ptr.h"
#import "components/unified_consent/pref_names.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_consumer.h"

@interface PrivacyGuideURLUsageMediator () <BooleanObserver>
@end

@implementation PrivacyGuideURLUsageMediator {
  raw_ptr<PrefService> _userPrefService;
  PrefBackedBoolean* _URLUsagePreference;
}

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    _userPrefService = userPrefService;
    _URLUsagePreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:unified_consent::prefs::
                                kUrlKeyedAnonymizedDataCollectionEnabled];
    _URLUsagePreference.observer = self;
  }
  return self;
}

- (void)setConsumer:(id<PrivacyGuideURLUsageConsumer>)consumer {
  _consumer = consumer;
  [_consumer setURLUsageEnabled:_URLUsagePreference.value];
}

- (void)disconnect {
  _URLUsagePreference = nil;
}

#pragma mark - PrivacyGuideURLUsageViewControllerDelegate

- (void)didEnableURLUsage:(BOOL)enabled {
  _URLUsagePreference.value = enabled;
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  CHECK_EQ(observableBoolean, _URLUsagePreference);
  [_consumer setURLUsageEnabled:_URLUsagePreference.value];
}

@end
