// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_consumer.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol AutofillCommands;
@protocol AutofillAISaveEntityMutator;

// View controller for the Autofill AI entity save and update detailed UI.
@interface AutofillAISaveEntityTableViewController
    : ChromeTableViewController <AutofillAISaveEntityConsumer>

// Autofill commands handler.
@property(nonatomic, weak) id<AutofillCommands> autofillHandler;

// Mutator for sending user actions to the mediator.
@property(nonatomic, weak) id<AutofillAISaveEntityMutator> mutator;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_TABLE_VIEW_CONTROLLER_H_
