// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_view_controller_protocol.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"

@protocol GeminiConsentMutator;

// Gemini consent View Controller (VC).
@interface GeminiConsentViewController
    : UIViewController <GeminiFirstRunViewControllerProtocol,
                        GeminiFirstRunStep>

// Initializer with the layout configuration.
- (instancetype)initWithConfiguration:(GeminiConsentConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// The mutator for this view controller to communicate to the mediator.
@property(nonatomic, weak) id<GeminiConsentMutator> mutator;

// The delegate to handle height changes and accordion toggles.
@property(nonatomic, weak) id<GeminiConsentViewControllerDelegate> delegate;

// The step delegate for the parent PageViewController.
@property(nonatomic, weak) id<GeminiFirstRunStepDelegate> stepDelegate;

// The Gemini First Run type being displayed.
@property(nonatomic, assign) GeminiFirstRunType firstRunType;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_VIEW_CONTROLLER_H_
