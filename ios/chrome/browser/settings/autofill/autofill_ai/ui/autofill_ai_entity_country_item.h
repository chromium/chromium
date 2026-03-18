// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_COUNTRY_ITEM_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_COUNTRY_ITEM_H_

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"

// Item for displaying and selecting a country in Autofill AI entity editing.
@interface AutofillAIEntityCountryItem : TableViewDetailIconItem

// The type of the attribute being edited.
@property(nonatomic, assign) autofill::AttributeTypeName attributeType;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ENTITY_COUNTRY_ITEM_H_
