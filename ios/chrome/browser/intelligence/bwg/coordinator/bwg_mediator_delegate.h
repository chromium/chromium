// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_MEDIATOR_DELEGATE_H_

@class BWGConsentMediator;

// Delegate for the BWGMediator.
@protocol BWGMediatorDelegate

// Potentially presents the BWG first run experience (FRE) based on eligibility
// such as if the FRE promo was shown. Returns YES if the BWG FRE was
// presented.
- (BOOL)maybePresentBWGFRE;

// Dismisses the BWG consent UI.
- (void)dismissBWGConsentUI;

// Decides whether BWG consent should be shown.
- (BOOL)shouldShowBWGConsent;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_MEDIATOR_DELEGATE_H_
