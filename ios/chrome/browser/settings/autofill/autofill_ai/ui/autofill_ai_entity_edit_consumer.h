// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_

#import <Foundation/Foundation.h>

namespace autofill {
class EntityInstance;
}

@class TableViewItem;

// The consumer of the Autofill AI entity view and edit mediator.
@protocol AutofillAIEntityEditConsumer <NSObject>

// Sets the entity instance to be viewed and edited.
- (void)setEntityInstance:(const autofill::EntityInstance&)entityInstance;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_
