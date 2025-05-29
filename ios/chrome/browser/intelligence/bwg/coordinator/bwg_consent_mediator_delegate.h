// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_CONSENT_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_CONSENT_MEDIATOR_DELEGATE_H_

@class BWGConsentMediator;

// Delegate for the BWGConsentMediator.
@protocol BWGConsentMediatorDelegate

// Dismisses the BWG consent UI.
- (void)dismissBWGConsentUI;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_BWG_CONSENT_MEDIATOR_DELEGATE_H_
