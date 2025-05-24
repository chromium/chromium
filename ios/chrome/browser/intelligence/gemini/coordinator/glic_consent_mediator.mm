// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_consent_mediator.h"

#import <memory>

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_consent_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/gemini/metrics/glic_metrics.h"
#import "ios/chrome/browser/intelligence/gemini/ui/glic_consent_view_controller.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@implementation GLICConsentMediator {
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
  }
  return self;
}

#pragma mark - GLICConsentMutator

// Did consent to GLIC.
- (void)didConsentGLIC {
  base::UmaHistogramEnumeration(kGLICConsentTypeHistogram,
                                GLICConsentType::kAccept);
  _prefService->SetBoolean(prefs::kIOSGLICConsent, YES);
  [_delegate dismissGLICConsentUI];
}

// Did dismisses the Consent UI.
- (void)didRefuseGLICConsent {
  base::UmaHistogramEnumeration(kGLICConsentTypeHistogram,
                                GLICConsentType::kCancel);
  [_delegate dismissGLICConsentUI];
}

// Did close GLIC Promo UI.
- (void)didCloseGLICPromo {
  [_delegate dismissGLICConsentUI];
}

@end
