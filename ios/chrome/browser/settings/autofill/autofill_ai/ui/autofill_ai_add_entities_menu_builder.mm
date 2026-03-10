// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_add_entities_menu_builder.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation AutofillAIAddEntitiesMenuBuilder

+ (UIMenu*)buildMenuWithTypes:(const std::vector<autofill::EntityType>&)types
                     delegate:(id<AutofillAIAddEntitiesMenuDelegate>)delegate {
  __weak id<AutofillAIAddEntitiesMenuDelegate> weakDelegate = delegate;

  NSMutableArray<UIAction*>* actions = [[NSMutableArray alloc] init];

  // Address
  UIAction* addressAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_AUTOFILL_ADD_ADDRESS_BUTTON_TEXT)
                          image:DefaultSymbolWithPointSize(
                                    kEnvelopeSymbol, kSymbolActionPointSize)
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakDelegate didSelectAddAutofillProfile];
                        }];
  [actions addObject:addressAction];

  for (const auto& entityType : types) {
    NSString* title = base::SysUTF16ToNSString(entityType.GetNameForI18n());
    UIImage* image = autofill::DefaultIconForAutofillAiEntityType(
        entityType.name(), kSymbolActionPointSize);

    UIAction* uiAction = [UIAction
        actionWithTitle:title
                  image:image
             identifier:nil
                handler:^(UIAction* action) {
                  [weakDelegate didSelectAddEntityWithType:entityType];
                }];
    [actions addObject:uiAction];
  }

  // When UIMenu is presented from a bottom toolbar, iOS draws the first array
  // element at the bottom (closest to the anchor). Reversing the array ensures
  // Address rendering explicitly at the top and subsequent entities rendering
  // downwards.
  return [UIMenu menuWithTitle:@""
                      children:[[actions reverseObjectEnumerator] allObjects]];
}

@end
