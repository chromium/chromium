// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_add_entities_menu_builder.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using AutofillAIAddEntitiesMenuBuilderTest = PlatformTest;

// Tests that the menu items are built in reverse order and enabled when
// entities are enabled.
TEST_F(AutofillAIAddEntitiesMenuBuilderTest, BuildsMenuInReverseOrder) {
  autofill::EntityType vehicle(autofill::EntityTypeName::kVehicle);
  autofill::EntityType passport(autofill::EntityTypeName::kPassport);
  std::vector<autofill::EntityType> entityTypes = {vehicle, passport};

  UIMenu* menu =
      [AutofillAIAddEntitiesMenuBuilder buildMenuWithTypes:entityTypes
                                           entitiesEnabled:YES
                                                  delegate:nil];

  NSArray<UIMenuElement*>* children = menu.children;
  ASSERT_EQ(children.count, 2u);

  UIAction* first_action =
      base::apple::ObjCCastStrict<UIAction>(children.firstObject);
  UIAction* last_action =
      base::apple::ObjCCastStrict<UIAction>(children.lastObject);
  EXPECT_NSEQ(base::SysUTF16ToNSString(passport.GetNameForI18n()),
              first_action.title);
  EXPECT_NSEQ(base::SysUTF16ToNSString(vehicle.GetNameForI18n()),
              last_action.title);
  EXPECT_EQ(first_action.attributes & UIMenuElementAttributesDisabled, 0u);
  EXPECT_EQ(last_action.attributes & UIMenuElementAttributesDisabled, 0u);
}

// Tests that all menu items are disabled when entities are disabled.
TEST_F(AutofillAIAddEntitiesMenuBuilderTest,
       DisablesItemsWhenEntitiesDisabled) {
  std::vector<autofill::EntityType> entityTypes = {
      autofill::EntityType(autofill::EntityTypeName::kVehicle),
      autofill::EntityType(autofill::EntityTypeName::kPassport)};

  UIMenu* menu =
      [AutofillAIAddEntitiesMenuBuilder buildMenuWithTypes:entityTypes
                                           entitiesEnabled:NO
                                                  delegate:nil];

  NSArray<UIMenuElement*>* children = menu.children;
  ASSERT_EQ(children.count, 2u);
  for (UIMenuElement* child in children) {
    UIAction* action = base::apple::ObjCCastStrict<UIAction>(child);
    EXPECT_NE(action.attributes & UIMenuElementAttributesDisabled, 0u);
  }
}
