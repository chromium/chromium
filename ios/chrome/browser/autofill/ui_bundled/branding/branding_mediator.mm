// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_consumer.h"

@implementation BrandingMediator {
  // Weak pointer to the local state that stores the number of times the
  // branding has shown and animated.
  raw_ptr<PrefService> _localState;
}

- (instancetype)initWithLocalState:(PrefService*)localState {
  self = [super init];
  if (self) {
    _localState = localState;
  }
  return self;
}

- (void)setConsumer:(id<BrandingConsumer>)consumer {
  _consumer = consumer;
  // Initial set up of the consumer.
  _consumer.visible =
      _localState->GetInteger(prefs::kAutofillBrandingIconDisplayCount) < 2;
  _consumer.shouldPerformPopAnimation =
      _localState->GetInteger(
          prefs::kAutofillBrandingIconAnimationRemainingCount) > 0;
}

- (void)disconnect {
  _localState = nil;
}

#pragma mark - BrandingViewControllerDelegate

- (void)brandingIconDidPress {
  base::RecordAction(base::UserMetricsAction("Autofill_BrandingTapped"));
}

- (void)brandingIconDidShow {
  int displayCount =
      _localState->GetInteger(prefs::kAutofillBrandingIconDisplayCount) + 1;
  _localState->SetInteger(prefs::kAutofillBrandingIconDisplayCount,
                          displayCount);
  self.consumer.visible = displayCount < 2;
}

- (void)brandingIconDidPerformPopAnimation {
  int popAnimationRemainingCount = _localState->GetInteger(
      prefs::kAutofillBrandingIconAnimationRemainingCount);
  popAnimationRemainingCount -= 1;
  self.consumer.shouldPerformPopAnimation = popAnimationRemainingCount > 0;
  _localState->SetInteger(prefs::kAutofillBrandingIconAnimationRemainingCount,
                          popAnimationRemainingCount);
}

@end
