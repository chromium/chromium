// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_cell_content_configuration.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

NSString* const kTestAttributeName = @"Passport Number";
NSString* const kTestAttributeValue = @"1234";

// Recursively searches `view` and its subviews for a view with
// `accessibility_id`.
UIView* FindViewById(UIView* view, NSString* accessibility_id) {
  if ([view.accessibilityIdentifier isEqualToString:accessibility_id]) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* found_view = FindViewById(subview, accessibility_id);
    if (found_view) {
      return found_view;
    }
  }
  return nil;
}

}  // namespace

using AtMemoryGranularFillCellContentConfigurationTest = PlatformTest;

// Tests that the content configuration initializes with the given properties
// and correctly generates and populates its content view.
TEST_F(AtMemoryGranularFillCellContentConfigurationTest,
       TestContentViewCreationAndConfiguration) {
  AtMemoryGranularFillCellContentConfiguration* config =
      [AtMemoryGranularFillCellContentConfiguration cellConfiguration];
  config.attributeName = kTestAttributeName;
  config.attributeValue = kTestAttributeValue;

  UIView<UIContentView>* contentView = [config makeContentView];
  ASSERT_NE(contentView, nil);

  id<UIContentConfiguration> appliedConfig = contentView.configuration;
  AtMemoryGranularFillCellContentConfiguration* appliedCellConfig =
      base::apple::ObjCCast<AtMemoryGranularFillCellContentConfiguration>(
          appliedConfig);
  ASSERT_NE(appliedCellConfig, nil);
  EXPECT_NSEQ(appliedCellConfig.attributeName, kTestAttributeName);
  EXPECT_NSEQ(appliedCellConfig.attributeValue, kTestAttributeValue);
}

// Tests that tapping the value chip button triggers the selection handler.
TEST_F(AtMemoryGranularFillCellContentConfigurationTest,
       TestSelectionHandlerTriggered) {
  AtMemoryGranularFillCellContentConfiguration* config =
      [AtMemoryGranularFillCellContentConfiguration cellConfiguration];
  config.attributeName = kTestAttributeName;
  config.attributeValue = kTestAttributeValue;

  __block NSString* selectedValue = nil;
  config.selectionHandler = ^(NSString* value) {
    selectedValue = value;
  };

  UIView<UIContentView>* contentView = [config makeContentView];
  ASSERT_NE(contentView, nil);

  NSString* button_id =
      GetAtMemoryGranularFillChipButtonAccessibilityIdentifier(
          kTestAttributeName);
  UIButton* chip_button =
      base::apple::ObjCCast<UIButton>(FindViewById(contentView, button_id));
  ASSERT_NE(chip_button, nil);
  [chip_button sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_NSEQ(selectedValue, kTestAttributeValue);
}

// Tests that `copyWithZone:` creates an equivalent copy of the configuration.
TEST_F(AtMemoryGranularFillCellContentConfigurationTest, TestCopying) {
  AtMemoryGranularFillCellContentConfiguration* config =
      [AtMemoryGranularFillCellContentConfiguration cellConfiguration];
  config.attributeName = kTestAttributeName;
  config.attributeValue = kTestAttributeValue;
  config.selectionHandler = ^(NSString* value) {
  };

  AtMemoryGranularFillCellContentConfiguration* copy = [config copy];
  ASSERT_NE(copy, nil);
  EXPECT_NSEQ(copy.attributeName, config.attributeName);
  EXPECT_NSEQ(copy.attributeValue, config.attributeValue);
  EXPECT_EQ(copy.selectionHandler, config.selectionHandler);
}
