// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_private_inference_notice_mutator.h"

@protocol AutofillCommands;
@protocol SettingsCommands;
class PrefService;

// Mediator for the Autofill AI Private Inference notice bottom sheet.
@interface AutofillAIPrivateInferenceNoticeMediator
    : NSObject <AutofillAIPrivateInferenceNoticeMutator>

// Initializes the mediator with `prefService`, `autofillHandler`, and
// `settingsHandler`.
- (instancetype)initWithPrefService:(PrefService*)prefService
                    autofillHandler:(id<AutofillCommands>)autofillHandler
                    settingsHandler:(id<SettingsCommands>)settingsHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_PRIVATE_INFERENCE_NOTICE_MEDIATOR_H_
