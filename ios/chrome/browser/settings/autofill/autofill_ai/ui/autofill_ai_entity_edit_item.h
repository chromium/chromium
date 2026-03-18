// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_ITEM_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_ITEM_H_

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"

// Item for editing Autofill AI entity fields.
@interface AutofillAIEntityEditItem : TableViewTextEditItem

// The type of the attribute being edited.
@property(nonatomic, assign) autofill::AttributeTypeName attributeType;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_EDIT_ITEM_H_
