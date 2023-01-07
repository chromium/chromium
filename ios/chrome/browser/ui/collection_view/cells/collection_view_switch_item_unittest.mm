// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using CollectionViewSwitchItemTest = PlatformTest;

// Tests that the label and switch values are set properly after a call to
// `configureCell:`.
TEST_F(CollectionViewSwitchItemTest, ConfigureCell) {
  CollectionViewSwitchItem* item =
      [[CollectionViewSwitchItem alloc] initWithType:0];
  NSString* text = @"Test Switch";

  item.text = text;
  item.on = YES;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[CollectionViewSwitchCell class]]);

  CollectionViewSwitchCell* switchCell =
      static_cast<CollectionViewSwitchCell*>(cell);
  EXPECT_FALSE(switchCell.textLabel.text);
  EXPECT_FALSE(switchCell.switchView.isOn);

  [item configureCell:cell];
  EXPECT_NSEQ(text, switchCell.textLabel.text);
  EXPECT_TRUE(switchCell.switchView.isOn);
}

// Tests that the text color and enabled state of the switch are set correctly
// by a call to `configureCell:`.
TEST_F(CollectionViewSwitchItemTest, EnabledAndDisabled) {
  CollectionViewSwitchCell* cell = [[CollectionViewSwitchCell alloc] init];
  CollectionViewSwitchItem* item =
      [[CollectionViewSwitchItem alloc] initWithType:0];
  item.text = @"Test Switch";

  // Text color possibilities.
  UIColor* enabledColor =
      [CollectionViewSwitchCell defaultTextColorForState:UIControlStateNormal];
  UIColor* disabledColor = [CollectionViewSwitchCell
      defaultTextColorForState:UIControlStateDisabled];

  // Enabled and off.
  item.on = NO;
  item.enabled = YES;
  [item configureCell:cell];
  EXPECT_FALSE(cell.switchView.isOn);
  EXPECT_NSEQ(enabledColor, cell.textLabel.textColor);

  // Enabled and on.
  item.on = YES;
  item.enabled = YES;
  [item configureCell:cell];
  EXPECT_TRUE(cell.switchView.isOn);
  EXPECT_NSEQ(enabledColor, cell.textLabel.textColor);

  // Disabled and off.
  item.on = NO;
  item.enabled = NO;
  [item configureCell:cell];
  EXPECT_FALSE(cell.switchView.isOn);
  EXPECT_NSEQ(disabledColor, cell.textLabel.textColor);

  // Disabled and on.
  item.on = YES;
  item.enabled = NO;
  [item configureCell:cell];
  EXPECT_TRUE(cell.switchView.isOn);
  EXPECT_NSEQ(disabledColor, cell.textLabel.textColor);
}

TEST_F(CollectionViewSwitchItemTest, PrepareForReuseClearsActions) {
  CollectionViewSwitchCell* cell = [[CollectionViewSwitchCell alloc] init];
  UISwitch* switchView = cell.switchView;
  NSArray* target = [NSArray array];

  EXPECT_EQ(0U, [[switchView allTargets] count]);
  [switchView addTarget:target
                 action:@selector(count)
       forControlEvents:UIControlEventValueChanged];
  EXPECT_EQ(1U, [[switchView allTargets] count]);

  [cell prepareForReuse];
  EXPECT_EQ(0U, [[switchView allTargets] count]);
}

}  // namespace
