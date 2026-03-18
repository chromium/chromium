// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_FAKE_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_FAKE_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_consumer.h"

@class TableViewItem;

// Fake consumer for AutofillAIEntityEditMediator and Coordinator tests.
@interface FakeAutofillAIEntityEditConsumer
    : NSObject <AutofillAIEntityEditConsumer>

// Title of the view controller.
@property(nonatomic, copy) NSString* title;

// Items to be displayed.
@property(nonatomic, strong) NSArray<TableViewItem*>* editItems;

// Whether editing is allowed.
@property(nonatomic, assign) BOOL editingAllowed;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_COORDINATOR_FAKE_AUTOFILL_AI_ENTITY_EDIT_CONSUMER_H_
