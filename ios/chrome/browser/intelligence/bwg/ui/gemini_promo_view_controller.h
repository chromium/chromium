// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_PROMO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_PROMO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_view_controller_protocol.h"

@protocol GeminiPromoMutator;

// Gemini promo View Controller.
@interface GeminiPromoViewController
    : UIViewController <GeminiFirstRunViewControllerProtocol,
                        GeminiFirstRunStep>

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<GeminiPromoMutator> mutator;

// The step delegate for the parent PageViewController.
@property(nonatomic, weak) id<GeminiFirstRunStepDelegate> stepDelegate;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_PROMO_VIEW_CONTROLLER_H_
