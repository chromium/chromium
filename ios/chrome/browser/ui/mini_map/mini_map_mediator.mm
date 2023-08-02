// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/mini_map/mini_map_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/web/annotations/annotations_util.h"
#import "ios/web/public/annotations/annotations_text_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The type of address annotations.
const char* const kDecorationAddress = "ADDRESS";
}  // namespace

@interface MiniMapMediator ()

@property(nonatomic, assign) PrefService* prefService;

// The WebState that triggered the request.
@property(assign) base::WeakPtr<web::WebState> webState;

@end

@implementation MiniMapMediator

- (instancetype)initWithPrefs:(PrefService*)prefService
                     webState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _prefService = prefService;
    if (webState) {
      _webState = webState->GetWeakPtr();
    }
  }
  return self;
}

- (void)disconnect {
  self.prefService = nil;
  self.webState = nullptr;
}

- (void)userInitiatedMiniMapConsentRequired:(BOOL)consentRequired {
  if (!self.prefService) {
    return;
  }

  if (consentRequired && ShouldPresentConsentScreen(self.prefService)) {
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

  if (self.webState) {
    auto* manager =
        web::AnnotationsTextManager::FromWebState(self.webState.get());
    if (manager) {
      manager->RemoveDecorationsWithType(kDecorationAddress);
    }
  }

  // TODO(crbug.com/1351353): disable address annotations.
}

@end
