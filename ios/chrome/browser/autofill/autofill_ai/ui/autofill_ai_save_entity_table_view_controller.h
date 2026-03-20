// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_TABLE_VIEW_CONTROLLER_H_

#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@protocol AutofillCommands;
@protocol AutofillAISaveEntityMutator;

// View controller for the Autofill AI entity save and update detailed UI.
@interface AutofillAISaveEntityTableViewController : ChromeTableViewController

// Sets the entities to be displayed. Called by the parent container view
// controller.
- (void)setNewEntity:(autofill::EntityInstance)newEntity
           oldEntity:(std::optional<autofill::EntityInstance>)oldEntity
           userEmail:(const std::u16string&)userEmail;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_TABLE_VIEW_CONTROLLER_H_
