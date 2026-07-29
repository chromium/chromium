// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_FIRST_RUN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_FIRST_RUN_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_step.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"

@protocol SceneCommands;
class AuthenticationService;
class GeminiService;
class PrefService;
class WebStateList;

namespace signin {
class IdentityManager;
}  // namespace signin

@protocol GeminiFirstRunMediatorDelegate;
@class GeminiConsentConfiguration;

// Gemini First Run Mediator.
@interface GeminiFirstRunMediator : NSObject <GeminiFirstRunMutator>

// The delegate for this mediator.
@property(nonatomic, weak) id<GeminiFirstRunMediatorDelegate> delegate;
// The handler for sending scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;
// Returns YES if the Gemini promo should be shown.
@property(nonatomic, readonly) BOOL shouldShowPromo;
// Returns YES if the AI Hub IPH should be shown.
@property(nonatomic, readonly) BOOL shouldShowAIHubIPH;
// Returns YES if the UI must enforce strict legal consent requirements.
@property(nonatomic, readonly) BOOL useStrictLegalConsent;

- (instancetype)initWithPrefService:(PrefService*)prefService
                       webStateList:(WebStateList*)webStateList
                 baseViewController:(UIViewController*)baseViewController
                      geminiService:(GeminiService*)geminiService
              authenticationService:(AuthenticationService*)authService
                    identityManager:(signin::IdentityManager*)identityManager
                            tracker:(feature_engagement::Tracker*)tracker
                         entryPoint:(gemini::EntryPoint)entryPoint
                  completionHandler:(void (^)(BOOL success))completion;

// Returns the consent configuration for the given First Run type.
- (GeminiConsentConfiguration*)consentConfigurationForFirstRunType:
    (GeminiFirstRunType)firstRunType;

// Returns whether to show the promo for the given First Run type.
- (BOOL)shouldShowPromoForFirstRunType:(GeminiFirstRunType)firstRunType;

// Returns whether to show the branding header for the given First Run type.
- (BOOL)shouldShowBrandingHeaderForFirstRunType:
    (GeminiFirstRunType)firstRunType;

// Returns the ordered list of steps to show for the given First Run type.
- (std::vector<GeminiFirstRunStepIdentifier>)stepsForFirstRunType:
    (GeminiFirstRunType)firstRunType;

// Disconnects the mediator, firing the completion callback with failure.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_FIRST_RUN_MEDIATOR_H_
