// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_mediator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"

@interface SuggestionsFromGeminiMediator () <BooleanObserver>
@end

@implementation SuggestionsFromGeminiMediator {
  // The observable boolean backing the preference status of the Suggestions
  // from Gemini toggle.
  PrefBackedBoolean* _personalContextSwitchEnabled;
}

- (instancetype)initWithPrefBackedBoolean:
    (PrefBackedBoolean*)personalContextSwitchEnabled {
  self = [super init];
  if (self) {
    CHECK(personalContextSwitchEnabled);
    _personalContextSwitchEnabled = personalContextSwitchEnabled;
    _personalContextSwitchEnabled.observer = self;
  }
  return self;
}

- (void)setConsumer:(id<SuggestionsFromGeminiConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [_consumer
        setSuggestionsFromGeminiSwitchOn:_personalContextSwitchEnabled.value];
  }
}

- (void)disconnect {
  _personalContextSwitchEnabled.observer = nil;
  _personalContextSwitchEnabled = nil;
}

#pragma mark - SuggestionsFromGeminiMutator

- (void)didToggleSuggestionsFromGeminiSwitch:(BOOL)on {
  _personalContextSwitchEnabled.value = on;
}

- (void)didSelectManageConnectedApps {
  [self.delegate suggestionsFromGeminiMediatorOpenConnectedApps:self];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(PrefBackedBoolean*)boolean {
  CHECK_EQ(boolean, _personalContextSwitchEnabled);
  [self.consumer
      setSuggestionsFromGeminiSwitchOn:_personalContextSwitchEnabled.value];
}

@end
