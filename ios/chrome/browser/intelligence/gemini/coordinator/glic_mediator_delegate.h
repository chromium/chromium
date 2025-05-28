// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_MEDIATOR_DELEGATE_H_

@class GLICConsentMediator;

// Delegate for the GLICMediator.
@protocol GLICMediatorDelegate

// Potentially presents the Glic first run experience(FRE) based on eligibility
// such as if the FRE promo was shown. Returns YES if the Glic FRE was
// presented.
- (BOOL)maybePresentGlicFRE;

// Dismisses the GLIC consent UI.
- (void)dismissGLICConsentUI;

// Decides whether GLIC consent should be shown.
- (BOOL)shouldShowGLICConsent;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_MEDIATOR_DELEGATE_H_
