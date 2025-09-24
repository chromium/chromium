// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/address_bar_preference/address_bar_preference_mediator.h"

#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/ui_bundled/address_bar_preference/address_bar_preference_consumer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

@interface AddressBarPreferenceMediator () <BooleanObserver> {
  PrefBackedBoolean* _bottomOmniboxEnabled;
}
@end

@implementation AddressBarPreferenceMediator

- (instancetype)init {
  self = [super init];
  if (self) {
    _bottomOmniboxEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:omnibox::kIsOmniboxInBottomPosition];
    [_bottomOmniboxEnabled setObserver:self];
  }
  return self;
}

- (void)disconnect {
  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<AddressBarPreferenceConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setPreferenceForOmniboxAtBottom:[_bottomOmniboxEnabled value]];
}

#pragma mark - AddressBarPreferenceServiceDelegate

- (void)didSelectTopAddressBarPreference {
  [_bottomOmniboxEnabled setValue:NO];
  [self.consumer setPreferenceForOmniboxAtBottom:NO];
}

- (void)didSelectBottomAddressBarPreference {
  [_bottomOmniboxEnabled setValue:YES];
  [self.consumer setPreferenceForOmniboxAtBottom:YES];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  DCHECK(observableBoolean == _bottomOmniboxEnabled);
  [self.consumer setPreferenceForOmniboxAtBottom:[observableBoolean value]];
}

@end
