// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace autofill {
class EntityDataManager;
class EntityInstance;
}  // namespace autofill

@protocol AutofillAIEntityEditConsumer;

@interface AutofillAIEntityEditMediator : NSObject

// The consumer of this mediator.
@property(nonatomic, weak) id<AutofillAIEntityEditConsumer> consumer;

- (instancetype)initWithEntityInstance:(autofill::EntityInstance)entityInstance
                     entityDataManager:
                         (autofill::EntityDataManager*)entityDataManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_AUTOFILL_AI_ENTITY_EDIT_MEDIATOR_H_
