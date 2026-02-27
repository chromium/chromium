// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_SAVE_ENTITY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_SAVE_ENTITY_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/autofill_ai/coordinator/autofill_ai_save_entity_mutator.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/save_entity_params.h"

@protocol AutofillAISaveEntityConsumer;

// Mediator for the Autofill AI entity save and update UI.
@interface AutofillAISaveEntityMediator : NSObject <AutofillAISaveEntityMutator>

// Consumer for this mediator.
@property(nonatomic, weak) id<AutofillAISaveEntityConsumer> consumer;

// Initializes the mediator with `SaveEntityParams`.
- (instancetype)initWithParams:(autofill::SaveEntityParams)params
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator and handles any cleanup.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_SAVE_ENTITY_MEDIATOR_H_
