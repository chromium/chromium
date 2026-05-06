// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class GeminiConsentViewController;

// Delegate protocol to handle height changes and accordion toggles.
@protocol GeminiConsentViewControllerDelegate <NSObject>

// Called when an accordion item is expanded.
- (void)consentViewControllerDidExpandAccordionItem:
    (GeminiConsentViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONSENT_VIEW_CONTROLLER_DELEGATE_H_
