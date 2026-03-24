// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_FIELD_ITEM_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_FIELD_ITEM_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"

// Protocol for items used in Autofill AI entity editing.
@protocol AutofillAIEntityFieldItem <NSObject>

// The type of the attribute being edited.
@property(nonatomic, assign) autofill::AttributeTypeName attributeType;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_FIELD_ITEM_H_
