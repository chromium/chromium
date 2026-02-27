// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_CONSUMER_H_

#import <Foundation/Foundation.h>

#import <optional>
#import <string>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

// Consumer for the Autofill AI entity save and update UI.
@protocol AutofillAISaveEntityConsumer <NSObject>

// Sets the entities to be displayed and managed by the consumer.
- (void)setNewEntity:(autofill::EntityInstance)newEntity
           oldEntity:(std::optional<autofill::EntityInstance>)oldEntity
           userEmail:(const std::u16string&)userEmail;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_CONSUMER_H_
