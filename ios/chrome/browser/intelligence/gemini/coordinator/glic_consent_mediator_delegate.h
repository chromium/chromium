// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_CONSENT_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_CONSENT_MEDIATOR_DELEGATE_H_

@class GLICConsentMediator;

// Delegate for the GLICConsentMediator.
@protocol GLICConsentMediatorDelegate

// Dismisses the GLIC consent UI.
- (void)dismissGLICConsentUI;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GEMINI_COORDINATOR_GLIC_CONSENT_MEDIATOR_DELEGATE_H_
