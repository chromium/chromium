// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_STEP_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_STEP_H_

#import <UIKit/UIKit.h>

@class ButtonStackConfiguration;
@protocol GeminiFirstRunStep;

// Identifiers for each individual step in the onboarding flow.
enum class GeminiFirstRunStepIdentifier {
  kPromo,
  kConsent,
};

// Delegate protocol for a step to notify the container of dynamic changes.
@protocol GeminiFirstRunStepDelegate <NSObject>

// Notifies the container that the step's height has changed dynamically.
- (void)stepContentHeightDidChange:(UIViewController<GeminiFirstRunStep>*)step;

@end

// Protocol that each onboarding screen controller must conform to in order to
// be paginated by `GeminiFirstRunPageViewController`.
@protocol GeminiFirstRunStep <NSObject>

@optional

// Delegate to notify the page container of size changes.
@property(nonatomic, weak) id<GeminiFirstRunStepDelegate> stepDelegate;

@required

// The unique identifier of the onboarding step.
- (GeminiFirstRunStepIdentifier)stepIdentifier;

// The layout and copy configuration for the action buttons at the bottom.
- (ButtonStackConfiguration*)buttonStackConfiguration;

// The current height of the step's visible content.
- (CGFloat)contentHeight;

// Called when the step is completing the presenting transition.
- (void)stepDidBecomeActive;

// Called when the step is about to transition away or be dismissed.
- (void)stepWillResignActive;

// Called when the primary action button is tapped.
- (void)didTapPrimaryButton;

// Called when the secondary action button is tapped.
- (void)didTapSecondaryButton;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_STEP_H_
