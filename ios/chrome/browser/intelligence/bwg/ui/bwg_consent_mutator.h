// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_CONSENT_MUTATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_CONSENT_MUTATOR_H_

#import "url/gurl.h"

// Mutator protocol for the view controller to communicate with the
// `BWGConsentMediator`.
@protocol BWGConsentMutator

// Did consent to BWG.
- (void)didConsentBWG;

// Did refuse BWG consent. Triggered by cancel.
- (void)didRefuseBWGConsent;

// Did close BWG Promo UI.
- (void)didCloseBWGPromo;

// Handles tap on learn about your choices.
- (void)openNewTabWithURL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_BWG_CONSENT_MUTATOR_H_
