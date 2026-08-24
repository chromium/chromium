// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_LIGHTWEIGHT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_LIGHTWEIGHT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"

@class GeminiConsentConfiguration;
@protocol GeminiFirstRunMutator;

// View controller for the Lightweight Gemini First Run Experience step.
@interface GeminiLightweightViewController
    : UIViewController <GeminiFirstRunStep>

// Mutator for delegating first run user actions.
@property(nonatomic, weak) id<GeminiFirstRunMutator> mutator;

// Delegate for notifying the container about dynamic size changes.
@property(nonatomic, weak) id<GeminiFirstRunStepDelegate> stepDelegate;

// Initializes the controller with the consent configuration.
- (instancetype)initWithConfiguration:(GeminiConsentConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_LIGHTWEIGHT_VIEW_CONTROLLER_H_
