// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_VISUAL_RICH_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_VISUAL_RICH_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"

@class GeminiConsentConfiguration;
@protocol GeminiFirstRunMutator;

// View controller for the Visual-Rich Gemini First Run Experience (FRE) step.
@interface GeminiVisualRichViewController
    : UIViewController <GeminiFirstRunStep>

// Mutator for dispatching first run user actions.
@property(nonatomic, weak) id<GeminiFirstRunMutator> mutator;

// Delegate for step size changes.
@property(nonatomic, weak) id<GeminiFirstRunStepDelegate> stepDelegate;

// Designated initializer.
- (instancetype)initWithConfiguration:(GeminiConsentConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_VISUAL_RICH_VIEW_CONTROLLER_H_
