// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_MUTATOR_H_

#import "url/gurl.h"

// Mutator protocol for the Gemini Promo step to communicate with the mediator.
@protocol GeminiPromoMutator <NSObject>

// Did close Gemini Promo UI.
- (void)didCloseGeminiPromo;

// Returns whether to show the Image Remix row in the promo.
- (BOOL)shouldShowImageRemixRow;

// Promo was shown.
- (void)didShowGeminiPromo;

@end

// Mutator protocol for the Gemini Consent step to communicate with the
// mediator.
@protocol GeminiConsentMutator <NSObject>

// Did consent to Gemini.
- (void)didConsentGemini;

// Did consent to Live Gemini.
- (void)didConsentToLiveGemini;

// Did refuse Gemini consent. Triggered by cancel.
- (void)didRefuseGeminiConsent;

// Handles tap on learn about your choices.
- (void)openNewTabWithURL:(const GURL&)URL;

@end

@protocol GeminiFirstRunMutator <GeminiPromoMutator, GeminiConsentMutator>
@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_FIRST_RUN_MUTATOR_H_
