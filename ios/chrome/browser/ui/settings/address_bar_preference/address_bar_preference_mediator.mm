// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/address_bar_preference/address_bar_preference_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/settings/address_bar_preference/address_bar_preference_consumer.h"

@implementation AddressBarPreferenceMediator {
  PrefService* _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - Properties

- (void)setConsumer:(id<AddressBarPreferenceConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setPreferenceForOmniboxAtBottom:_prefService->GetBoolean(
                                                     prefs::kBottomOmnibox)];
}

#pragma mark - AddressBarPreferenceServiceDelegate

- (void)didSelectTopAddressBarPreference {
  _prefService->SetBoolean(prefs::kBottomOmnibox, false);
  [self.consumer setPreferenceForOmniboxAtBottom:false];
}

- (void)didSelectBottomAddressBarPreference {
  _prefService->SetBoolean(prefs::kBottomOmnibox, true);
  [self.consumer setPreferenceForOmniboxAtBottom:true];
}

@end
