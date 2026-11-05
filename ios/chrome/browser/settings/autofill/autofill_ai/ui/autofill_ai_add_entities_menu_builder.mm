// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_add_entities_menu_builder.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

@implementation AutofillAIAddEntitiesMenuBuilder

+ (UIMenu*)buildMenuWithTypes:(const std::vector<autofill::EntityType>&)types
              entitiesEnabled:(BOOL)entitiesEnabled
                     delegate:(id<AutofillAIAddEntitiesMenuDelegate>)delegate {
  __weak id<AutofillAIAddEntitiesMenuDelegate> weakDelegate = delegate;

  NSMutableArray<UIAction*>* actions = [[NSMutableArray alloc] init];

  for (const auto& entityType : types) {
    NSString* title = base::SysUTF16ToNSString(entityType.GetNameForI18n());
    UIImage* image = autofill::DefaultIconForAutofillAiEntityType(
        entityType.name(), /*is_personal_context=*/false,
        kSymbolActionPointSize, /*tint_color=*/nil);

    autofill::EntityType capturedType = entityType;
    UIAction* uiAction = [UIAction
        actionWithTitle:title
                  image:image
             identifier:nil
                handler:^(UIAction* action) {
                  [weakDelegate didSelectAddEntityWithType:capturedType];
                }];
    if (!entitiesEnabled) {
      uiAction.attributes = UIMenuElementAttributesDisabled;
    }
    [actions addObject:uiAction];
  }

  // When UIMenu is presented from a bottom toolbar, iOS draws the first array
  // element at the bottom (closest to the anchor). Reversing the array ensures
  // the first entity type renders at the top and subsequent entities render
  // downwards.
  return [UIMenu menuWithTitle:@""
                      children:[[actions reverseObjectEnumerator] allObjects]];
}

@end
