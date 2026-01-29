// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_MUTATOR_H_

#import "url/gurl.h"

// Mutator protocol for the view controller to communicate with the
//  `GeminiFirstRunMediator`.
@protocol GeminiConsentMutator

// Did consent to Gemini.
- (void)didConsentGemini;

// Did refuse Gemini consent. Triggered by cancel.
- (void)didRefuseGeminiConsent;

// Did close Gemini Promo UI.
- (void)didCloseGeminiPromo;

// Handles tap on learn about your choices.
- (void)openNewTabWithURL:(const GURL&)URL;

// Promo was shown.
- (void)didShowGeminiPromo;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_MUTATOR_H_
