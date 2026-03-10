// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_add_entities_menu_builder.h"

#import "base/apple/foundation_util.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using AutofillAIAddEntitiesMenuBuilderTest = PlatformTest;

TEST_F(AutofillAIAddEntitiesMenuBuilderTest, AddressAtEndAndTotalItems) {
  std::vector<autofill::EntityType> entityTypes = {
      autofill::EntityType(autofill::EntityTypeName::kVehicle),
      autofill::EntityType(autofill::EntityTypeName::kPassport)};

  UIMenu* menu =
      [AutofillAIAddEntitiesMenuBuilder buildMenuWithTypes:entityTypes
                                                  delegate:nil];

  NSArray<UIMenuElement*>* children = menu.children;
  EXPECT_EQ(children.count, 3u);

  UIAction* last_action =
      base::apple::ObjCCastStrict<UIAction>(children.lastObject);
  EXPECT_TRUE([last_action.title
      isEqualToString:l10n_util::GetNSString(
                          IDS_IOS_AUTOFILL_ADD_ADDRESS_BUTTON_TEXT)]);
}
