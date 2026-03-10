// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ADD_ENTITIES_MENU_BUILDER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ADD_ENTITIES_MENU_BUILDER_H_

#import <UIKit/UIKit.h>

#import <optional>
#import <vector>

namespace autofill {
class EntityType;
}  // namespace autofill

// Protocol for the Autofill AI "Add Entities" context menu.
@protocol AutofillAIAddEntitiesMenuDelegate <NSObject>

// Called when "Address" is selected to be added.
- (void)didSelectAddAutofillProfile;

// Called when an entity type is selected to be added.
- (void)didSelectAddEntityWithType:(autofill::EntityType)type;

@end

// Builder for the Autofill AI "Add Entities" context menu.
@interface AutofillAIAddEntitiesMenuBuilder : NSObject

// Returns a UIMenu for adding an address or a new entity.
+ (UIMenu*)buildMenuWithTypes:(const std::vector<autofill::EntityType>&)types
                     delegate:(id<AutofillAIAddEntitiesMenuDelegate>)delegate;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_ADD_ENTITIES_MENU_BUILDER_H_
