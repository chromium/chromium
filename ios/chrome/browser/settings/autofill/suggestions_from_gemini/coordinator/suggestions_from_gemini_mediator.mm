// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"

@implementation SuggestionsFromGeminiMediator {
  raw_ptr<PrefService> _prefs;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    CHECK(prefService);
    _prefs = prefService;
  }
  return self;
}

- (void)disconnect {
  _prefs = nullptr;
}

#pragma mark - SuggestionsFromGeminiMutator

- (void)didSelectManageConnectedApps {
  [self.delegate suggestionsFromGeminiMediatorOpenConnectedApps:self];
}

@end
