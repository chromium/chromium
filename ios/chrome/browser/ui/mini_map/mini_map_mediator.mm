// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/mini_map/mini_map_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/web/annotations/annotations_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MiniMapMediator ()

@property(nonatomic, assign) PrefService* prefService;

@end

@implementation MiniMapMediator

- (instancetype)initWithPrefs:(PrefService*)prefService {
  self = [super init];
  if (self) {
    self.prefService = prefService;
  }
  return self;
}

- (void)disconnect {
  self.prefService = nil;
}

- (void)userInitiatedMiniMapConsentRequired:(BOOL)consentRequired {
  if (!self.prefService) {
    return;
  }

  if (consentRequired &&
      !IsAddressAutomaticDetectionAccepted(self.prefService)) {
    [self.delegate showConsentInterstitial];
    return;
  }
  [self.delegate showMap];
}

- (void)userConsented {
  if (!self.prefService) {
    return;
  }
  self.prefService->SetBoolean(prefs::kDetectAddressesAccepted, true);
  [self.delegate showMap];
}

- (void)userDeclined {
  if (!self.prefService) {
    return;
  }
  self.prefService->SetBoolean(prefs::kDetectAddressesAccepted, false);
  self.prefService->SetBoolean(prefs::kDetectAddressesEnabled, false);
  [self.delegate dismissConsentInterstitialWithCompletion:nil];

  // TODO(crbug.com/1351353): disable address annotations.
}

@end
