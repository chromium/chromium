// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_AUTOFILL_AI_UI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_AUTOFILL_AI_UI_UTIL_H_

#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"

namespace autofill {

// Returns the default icon for the Autofill AI entity type.
UIImage* DefaultIconForAutofillAiEntityType(EntityTypeName entity_type_name,
                                            CGFloat symbol_point_size);

// Returns the display name for the given Autofill AI attribute type. For most
// attribute types, the display name is the same as the attribute type name.
// For attribute types with long names, this function returns a localized
// shorter name.
NSString* DisplayNameForAutofillAiAttributeType(AttributeType attribute_type);

// Returns the title for a dialog asking to save an entity.
NSString* GetDialogTitleForSaveEntity(EntityTypeName entity_type_name);

// Returns the title for a dialog asking to update an entity.
NSString* GetDialogTitleForUpdateEntity(EntityTypeName entity_type_name);

// Returns the title for a dialog asking to add an entity.
NSString* GetDialogTitleForAddEntity(EntityTypeName entity_type_name);

// Returns the title for a dialog to view an entity.
NSString* GetDialogTitleForViewEntity(EntityTypeName entity_type_name);

// Returns the title for a dialog to edit an entity.
NSString* GetDialogTitleForEditEntity(EntityTypeName entity_type_name);

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_PUBLIC_AUTOFILL_AI_UI_UTIL_H_
