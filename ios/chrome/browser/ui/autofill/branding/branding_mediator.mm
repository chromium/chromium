// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/branding/branding_mediator.h"

#import "base/allocator/partition_allocator/pointers/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/autofill/branding/branding_consumer.h"
#import "ios/chrome/browser/ui/autofill/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BrandingMediator {
  // Weak pointer to the local state that stores the number of times the
  // branding has shown and animated.
  raw_ptr<PrefService> _localState;
  // Remaining count that the branding should animate on appearance.
  int _popAnimationRemainingCount;
}

- (instancetype)initWithLocalState:(PrefService*)localState {
  self = [super init];
  if (self) {
    _localState = localState;
    _popAnimationRemainingCount = _localState->GetInteger(
        prefs::kAutofillBrandingIconAnimationRemainingCountPrefName);
  }
  return self;
}

- (void)setConsumer:(id<BrandingConsumer>)consumer {
  _consumer = consumer;
  // Initial set up of the consumer.
  // TODO(crbug.com/1447909): Currently all frequency types other than ::kNever
  // are treated as ::kAlways. Implement logic for other frequency types.
  consumer.visible = autofill::features::GetAutofillBrandingFrequencyType() !=
                     autofill::features::AutofillBrandingFrequencyType::kNever;
  consumer.shouldPerformPopAnimation = _popAnimationRemainingCount > 0;
}

- (void)disconnect {
  _localState = nil;
}

#pragma mark - BrandingViewControllerDelegate

- (void)brandingIconPressed {
  base::RecordAction(base::UserMetricsAction("Autofill_BrandingTapped"));
}

- (void)brandingIconDidPerformPopAnimation {
  _popAnimationRemainingCount -= 1;
  self.consumer.shouldPerformPopAnimation = _popAnimationRemainingCount > 0;
  _localState->SetInteger(
      prefs::kAutofillBrandingIconAnimationRemainingCountPrefName,
      _popAnimationRemainingCount);
}

@end
