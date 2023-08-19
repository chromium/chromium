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

using autofill::features::AutofillBrandingFrequencyType;
using autofill::features::GetAutofillBrandingFrequencyType;

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
  switch (GetAutofillBrandingFrequencyType()) {
    case AutofillBrandingFrequencyType::kNever:
      _consumer.visible = NO;
      break;
    case AutofillBrandingFrequencyType::kTwice:
      _consumer.visible =
          _localState->GetInteger(prefs::kAutofillBrandingIconDisplayCount) < 2;
      break;
    case AutofillBrandingFrequencyType::kUntilInteracted:
    case AutofillBrandingFrequencyType::kDismissWhenInteracted:
      _consumer.visible = !_localState->GetBoolean(
          prefs::kAutofillBrandingKeyboardAccessoriesTapped);
      break;
    case AutofillBrandingFrequencyType::kAlwaysShowAndDismiss:
    case AutofillBrandingFrequencyType::kAlways:
      _consumer.visible = YES;
      break;
  }
  consumer.shouldPerformPopAnimation =
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
  int displayCount;
  switch (GetAutofillBrandingFrequencyType()) {
    case AutofillBrandingFrequencyType::kNever:
    case AutofillBrandingFrequencyType::kAlways:
    case AutofillBrandingFrequencyType::kDismissWhenInteracted:
    case AutofillBrandingFrequencyType::kUntilInteracted:
      break;
    case AutofillBrandingFrequencyType::kTwice:
      displayCount =
          _localState->GetInteger(prefs::kAutofillBrandingIconDisplayCount) + 1;
      _localState->SetInteger(prefs::kAutofillBrandingIconDisplayCount,
                              displayCount);
      self.consumer.visible = displayCount < 2;
      break;
    case AutofillBrandingFrequencyType::kAlwaysShowAndDismiss:
      if (!self.consumer.shouldPerformPopAnimation) {
        [self.consumer slideAwayFromLeadingEdge];
      }
      break;
  }
}

- (void)brandingIconDidPerformPopAnimation {
  int popAnimationRemainingCount = _localState->GetInteger(
      prefs::kAutofillBrandingIconAnimationRemainingCount);
  popAnimationRemainingCount -= 1;
  self.consumer.shouldPerformPopAnimation = popAnimationRemainingCount > 0;
  _localState->SetInteger(prefs::kAutofillBrandingIconAnimationRemainingCount,
                          popAnimationRemainingCount);
  if (autofill::features::GetAutofillBrandingFrequencyType() ==
      autofill::features::AutofillBrandingFrequencyType::
          kAlwaysShowAndDismiss) {
    [self.consumer slideAwayFromLeadingEdge];
  }
}

- (void)keyboardAccessoryDidTap {
  if (!self.consumer.visible) {
    return;
  }
  switch (GetAutofillBrandingFrequencyType()) {
    case AutofillBrandingFrequencyType::kNever:
    case AutofillBrandingFrequencyType::kTwice:
    case AutofillBrandingFrequencyType::kAlwaysShowAndDismiss:
    case AutofillBrandingFrequencyType::kAlways:
      break;
    case AutofillBrandingFrequencyType::kDismissWhenInteracted:
      [self.consumer slideAwayFromLeadingEdge];
      [[fallthrough]];
    case AutofillBrandingFrequencyType::kUntilInteracted:
      self.consumer.visible = NO;
      _localState->SetBoolean(prefs::kAutofillBrandingKeyboardAccessoriesTapped,
                              YES);
      break;
  }
}

@end
