// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_FIRST_RUN_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_FIRST_RUN_MEDIATOR_DELEGATE_H_

#import "base/ios/block_types.h"

// Delegate for GeminiFirstRunMediator.
@protocol GeminiFirstRunMediatorDelegate <NSObject>

// Dismisses the Gemini consent UI.
- (void)dismissGeminiConsentUIWithCompletion:(ProceduralBlock)completion;

// Dismisses the Gemini flow.
- (void)dismissGeminiFlow;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_FIRST_RUN_MEDIATOR_DELEGATE_H_
