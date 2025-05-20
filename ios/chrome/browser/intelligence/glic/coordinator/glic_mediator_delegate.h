// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_COORDINATOR_GLIC_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_COORDINATOR_GLIC_MEDIATOR_DELEGATE_H_

@class GLICConsentMediator;

// Delegate for the GLICMediator.
@protocol GLICMediatorDelegate

// Dismisses the GLIC consent UI.
- (void)dismissGLICConsentUI;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_COORDINATOR_GLIC_MEDIATOR_DELEGATE_H_
