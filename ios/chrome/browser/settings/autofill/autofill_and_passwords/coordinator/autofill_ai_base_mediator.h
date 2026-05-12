// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AI_BASE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AI_BASE_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_ai_base_mutator.h"

namespace autofill {
class EntityDataManager;
}

@class AutofillAIBaseMediator;
@class TableViewItem;

// Delegate for AutofillAIBaseMediator.
@protocol AutofillAIBaseMediatorDelegate <NSObject>

- (void)autofillAIBaseMediator:(AutofillAIBaseMediator*)mediator
    didRequestToOpenEntityWithID:(autofill::EntityInstance::EntityId)entityID;

@end

// Base mediator for Autofill AI settings pages that display lists of entity
// instances.
@interface AutofillAIBaseMediator : NSObject <AutofillAIBaseMutator>

// Delegate for this mediator.
@property(nonatomic, weak) id<AutofillAIBaseMediatorDelegate> delegate;

// Point size for AI entity icons.
+ (CGFloat)entityIconPointSize;

- (instancetype)initWithEntityDataManager:
    (autofill::EntityDataManager*)entityDataManager NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect NS_REQUIRES_SUPER;

// Fetches the relevant entity instances, creates their corresponding items, and
// updates the consumer.
- (void)pushEntitiesToConsumer;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AI_BASE_MEDIATOR_H_
