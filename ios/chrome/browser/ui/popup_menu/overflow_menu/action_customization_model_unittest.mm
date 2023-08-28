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

// Tests that hiding an action moves it to the hidden group, but keeps its
// overall position in the list.
TEST_F(ActionCustomizationModelTest, HidingActionMovesToHidden) {
  OverflowMenuAction* bookmark =
      CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark);
  OverflowMenuAction* textZoom =
      CreateOverflowMenuAction(overflow_menu::ActionType::TextZoom);
  model_ = [[ActionCustomizationModel alloc]
      initWithActions:@[ bookmark, textZoom ]];

  bookmark.shown = NO;

  ASSERT_EQ(model_.shownActions.count, 1u);
  EXPECT_EQ(model_.shownActions[0], textZoom);
  ASSERT_EQ(model_.hiddenActions.count, 1u);
  EXPECT_EQ(model_.hiddenActions[0], bookmark);
  ASSERT_EQ(model_.actionsGroup.actions.count, 2u);
  ASSERT_EQ(model_.actionsGroup.actions[0], bookmark);
  ASSERT_EQ(model_.actionsGroup.actions[1], textZoom);
}

// Tests that showing a hidden action moves it to the shown group, but keeps its
// overall position in the list.
TEST_F(ActionCustomizationModelTest, ShowingMovesToShown) {
  OverflowMenuAction* bookmark =
      CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark);
  bookmark.shown = NO;
  OverflowMenuAction* textZoom =
      CreateOverflowMenuAction(overflow_menu::ActionType::TextZoom);
  model_ = [[ActionCustomizationModel alloc]
      initWithActions:@[ textZoom, bookmark ]];

  bookmark.shown = YES;

  ASSERT_EQ(model_.shownActions.count, 2u);
  EXPECT_EQ(model_.shownActions[0], textZoom);
  EXPECT_EQ(model_.shownActions[1], bookmark);
  ASSERT_EQ(model_.hiddenActions.count, 0u);
  ASSERT_EQ(model_.actionsGroup.actions.count, 2u);
  ASSERT_EQ(model_.actionsGroup.actions[0], textZoom);
  ASSERT_EQ(model_.actionsGroup.actions[1], bookmark);
}

// Tests that hidden items remain in their original order in the hidden group.
TEST_F(ActionCustomizationModelTest, HidingMovesToEnd) {
  OverflowMenuAction* bookmark =
      CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark);
  OverflowMenuAction* textZoom =
      CreateOverflowMenuAction(overflow_menu::ActionType::TextZoom);
  OverflowMenuAction* follow =
      CreateOverflowMenuAction(overflow_menu::ActionType::Follow);
  model_ = [[ActionCustomizationModel alloc]
      initWithActions:@[ bookmark, textZoom, follow ]];

  follow.shown = NO;
  textZoom.shown = NO;

  ASSERT_EQ(model_.shownActions.count, 1u);
  EXPECT_EQ(model_.shownActions[0], bookmark);
  ASSERT_EQ(model_.hiddenActions.count, 2u);
  EXPECT_EQ(model_.hiddenActions[0], textZoom);
  EXPECT_EQ(model_.hiddenActions[1], follow);
  ASSERT_EQ(model_.actionsGroup.actions.count, 3u);
  ASSERT_EQ(model_.actionsGroup.actions[0], bookmark);
  ASSERT_EQ(model_.actionsGroup.actions[1], textZoom);
  ASSERT_EQ(model_.actionsGroup.actions[2], follow);
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

  ASSERT_EQ(model_.shownActions.count, 2u);
  EXPECT_EQ(model_.shownActions[0], bookmark);
  EXPECT_EQ(model_.shownActions[1], textZoom);
  ASSERT_EQ(model_.hiddenActions.count, 2u);
  EXPECT_EQ(model_.hiddenActions[0], follow);
  EXPECT_EQ(model_.hiddenActions[1], translate);
  ASSERT_EQ(model_.actionsGroup.actions.count, 4u);
  ASSERT_EQ(model_.actionsGroup.actions[0], bookmark);
  ASSERT_EQ(model_.actionsGroup.actions[1], follow);
  ASSERT_EQ(model_.actionsGroup.actions[2], textZoom);
  ASSERT_EQ(model_.actionsGroup.actions[3], translate);
}

// Tests that hiding and showing an action does not change its position.
TEST_F(ActionCustomizationModelTest, HidingAndShowingActionDoesntMove) {
  OverflowMenuAction* bookmark =
      CreateOverflowMenuAction(overflow_menu::ActionType::Bookmark);
  OverflowMenuAction* textZoom =
      CreateOverflowMenuAction(overflow_menu::ActionType::TextZoom);
  model_ = [[ActionCustomizationModel alloc]
      initWithActions:@[ bookmark, textZoom ]];

  bookmark.shown = NO;

  ASSERT_EQ(model_.shownActions.count, 1u);
  EXPECT_EQ(model_.shownActions[0], textZoom);
  ASSERT_EQ(model_.hiddenActions.count, 1u);
  EXPECT_EQ(model_.hiddenActions[0], bookmark);
  ASSERT_EQ(model_.actionsGroup.actions.count, 2u);
  ASSERT_EQ(model_.actionsGroup.actions[0], bookmark);
  ASSERT_EQ(model_.actionsGroup.actions[1], textZoom);

  bookmark.shown = YES;

  ASSERT_EQ(model_.shownActions.count, 2u);
  EXPECT_EQ(model_.shownActions[0], bookmark);
  EXPECT_EQ(model_.shownActions[1], textZoom);
  ASSERT_EQ(model_.hiddenActions.count, 0u);
  ASSERT_EQ(model_.actionsGroup.actions.count, 2u);
  ASSERT_EQ(model_.actionsGroup.actions[0], bookmark);
  ASSERT_EQ(model_.actionsGroup.actions[1], textZoom);
}
