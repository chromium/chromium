// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_settings_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_settings_consumer.h"

@implementation AutofillSettingsMediator {
  raw_ptr<PrefService> _userPrefService;
}

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService {
  self = [super init];
  if (self) {
    _userPrefService = userPrefService;
  }
  return self;
}

- (void)setConsumer:(id<AutofillSettingsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  if (_consumer && _userPrefService) {
    // TODO(crbug.com/491417627): Introduce future consumer calls.
  }
}

- (void)disconnect {
  _userPrefService = nullptr;
}

@end
