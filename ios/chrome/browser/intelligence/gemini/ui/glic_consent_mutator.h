// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_CONSENT_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_CONSENT_MUTATOR_H_

// Mutator protocol for the view controller to communicate with the
// `GLICConsentMediator`.
@protocol GLICConsentMutator

// Did consent to GLIC.
- (void)didConsentGLIC;

// Did refuse GLIC consent. Triggered by cancel.
- (void)didRefuseGLICConsent;

// Did close GLIC Promo UI.
- (void)didCloseGLICPromo;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_UI_GLIC_CONSENT_MUTATOR_H_
