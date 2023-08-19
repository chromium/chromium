// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

OverflowMenuAction* CreateOverflowMenuAction(
    overflow_menu::ActionType actionType) {
  OverflowMenuAction* result =
      [[OverflowMenuAction alloc] initWithName:@"Foobar"
                                    symbolName:kSettingsSymbol
                                  systemSymbol:YES
                              monochromeSymbol:NO
                       accessibilityIdentifier:@"Foobar"
                            enterpriseDisabled:NO
                           displayNewLabelIcon:NO
                                       handler:^{
                                           // Do nothing
                                       }];

  result.actionType = static_cast<NSInteger>(actionType);

  return result;
}

}  // namespace

class ActionCustomizationModelTest : public PlatformTest {
 public:
  ActionCustomizationModelTest() {}

 protected:
  ActionCustomizationModel* model_;
};

// Tests that hiding an action moves it to the hidden group.
TEST_F(ActionCustomizationModelTest, HidingActionMovesToHidden) {
  OverflowMenuAction* bookmark =
      CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark);
  OverflowMenuAction* textZoom =
      CreateOverflowMenuAction(overflow_menu::ActionType::TextZoom);
  model_ = [[ActionCustomizationModel alloc]
      initWithActions:@[ bookmark, textZoom ]];

  bookmark.shown = NO;

  ASSERT_EQ(model_.shownActions.actions.count, 1u);
  EXPECT_EQ(model_.shownActions.actions[0], textZoom);
  ASSERT_EQ(model_.hiddenActions.actions.count, 1u);
  EXPECT_EQ(model_.hiddenActions.actions[0], bookmark);
}

// Tests that showing a hidden action moves it to the shown group.
TEST_F(ActionCustomizationModelTest, ShowingMovesToShown) {
  OverflowMenuAction* bookmark =
      CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark);
  bookmark.shown = NO;
  OverflowMenuAction* textZoom =
      CreateOverflowMenuAction(overflow_menu::ActionType::TextZoom);
  model_ = [[ActionCustomizationModel alloc]
      initWithActions:@[ textZoom, bookmark ]];

  bookmark.shown = YES;

  ASSERT_EQ(model_.shownActions.actions.count, 2u);
  EXPECT_EQ(model_.shownActions.actions[0], textZoom);
  EXPECT_EQ(model_.shownActions.actions[1], bookmark);
  ASSERT_EQ(model_.hiddenActions.actions.count, 0u);
}

// Tests that items are moved to the end of opposite array one at a time, in
// order.
TEST_F(ActionCustomizationModelTest, HidingMovesToEnd) {
  OverflowMenuAction* bookmark =
      CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark);
  OverflowMenuAction* textZoom =
      CreateOverflowMenuAction(overflow_menu::ActionType::TextZoom);
  OverflowMenuAction* follow =
      CreateOverflowMenuAction(overflow_menu::ActionType::Follow);
  model_ = [[ActionCustomizationModel alloc]
      initWithActions:@[ bookmark, textZoom, follow ]];

  // Hide text zoom first, and then follow. Text zoom should be first in the
  // hidden list.
  textZoom.shown = NO;
  follow.shown = NO;

  ASSERT_EQ(model_.shownActions.actions.count, 1u);
  EXPECT_EQ(model_.shownActions.actions[0], bookmark);
  ASSERT_EQ(model_.hiddenActions.actions.count, 2u);
  EXPECT_EQ(model_.hiddenActions.actions[0], textZoom);
  EXPECT_EQ(model_.hiddenActions.actions[1], follow);
}

// Tests that if an action is hidden and then shown again, it ends up at the end
// of the shown array.
TEST_F(ActionCustomizationModelTest, HidingAndShowingMovesToEnd) {
  OverflowMenuAction* bookmark =
      CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark);
  OverflowMenuAction* textZoom =
      CreateOverflowMenuAction(overflow_menu::ActionType::TextZoom);
  OverflowMenuAction* follow =
      CreateOverflowMenuAction(overflow_menu::ActionType::Follow);
  model_ = [[ActionCustomizationModel alloc]
      initWithActions:@[ bookmark, textZoom, follow ]];

  bookmark.shown = NO;
  bookmark.shown = YES;

  ASSERT_EQ(model_.shownActions.actions.count, 3u);
  EXPECT_EQ(model_.shownActions.actions[0], textZoom);
  EXPECT_EQ(model_.shownActions.actions[1], follow);
  EXPECT_EQ(model_.shownActions.actions[2], bookmark);
  ASSERT_EQ(model_.hiddenActions.actions.count, 0u);
}

// Tests that the model partitions actions into shown and hidden halves
// correctly.
TEST_F(ActionCustomizationModelTest, ModelPartitionsActions) {
  OverflowMenuAction* bookmark =
      CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark);
  OverflowMenuAction* textZoom =
      CreateOverflowMenuAction(overflow_menu::ActionType::TextZoom);
  OverflowMenuAction* follow =
      CreateOverflowMenuAction(overflow_menu::ActionType::Follow);
  OverflowMenuAction* translate =
      CreateOverflowMenuAction(overflow_menu::ActionType::Translate);

  follow.shown = NO;
  translate.shown = NO;

  model_ = [[ActionCustomizationModel alloc]
      initWithActions:@[ bookmark, follow, textZoom, translate ]];

  ASSERT_EQ(model_.shownActions.actions.count, 2u);
  EXPECT_EQ(model_.shownActions.actions[0], bookmark);
  EXPECT_EQ(model_.shownActions.actions[1], textZoom);
  ASSERT_EQ(model_.hiddenActions.actions.count, 2u);
  EXPECT_EQ(model_.hiddenActions.actions[0], follow);
  EXPECT_EQ(model_.hiddenActions.actions[1], translate);
}
