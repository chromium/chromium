// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_consent_mediator.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_consent_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_view_controller.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation BWGConsentMediator {
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - BWGConsentMutator

// Did consent to BWG.
- (void)didConsentBWG {
  _prefService->SetBoolean(prefs::kIOSBwgConsent, YES);
  [_delegate dismissBWGConsentUI];
}

// Did dismisses the Consent UI.
- (void)didRefuseBWGConsent {
  [_delegate dismissBWGConsentUI];
}

// Did close BWG Promo UI.
- (void)didCloseBWGPromo {
  [_delegate dismissBWGConsentUI];
}

@end
