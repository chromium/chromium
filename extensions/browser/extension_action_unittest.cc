// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_action.h"

#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

std::unique_ptr<ExtensionAction> CreateAction(const ActionInfo& action_info) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  return std::make_unique<ExtensionAction>(*extension, action_info);
}

}  // namespace

TEST(ExtensionActionTest, Title) {
  ActionInfo action_info(ActionInfo::Type::kPage);
  action_info.default_title = "Initial Title";
  std::unique_ptr<ExtensionAction> action = CreateAction(action_info);

  ASSERT_EQ("Initial Title", action->GetTitle(1));
  action->SetTitle(ExtensionAction::kDefaultTabId, "foo");
  ASSERT_EQ("foo", action->GetTitle(1));
  ASSERT_EQ("foo", action->GetTitle(100));
  action->SetTitle(100, "bar");
  ASSERT_EQ("foo", action->GetTitle(1));
  ASSERT_EQ("bar", action->GetTitle(100));
  action->SetTitle(ExtensionAction::kDefaultTabId, "baz");
  ASSERT_EQ("baz", action->GetTitle(1));
  action->ClearAllValuesForTab(100);
  ASSERT_EQ("baz", action->GetTitle(100));
}

TEST(ExtensionActionTest, Visibility) {
  std::unique_ptr<ExtensionAction> action =
      CreateAction(ActionInfo(ActionInfo::Type::kPage));

  ASSERT_FALSE(action->GetIsVisible(1));
  action->SetIsVisible(ExtensionAction::kDefaultTabId, true);
  ASSERT_TRUE(action->GetIsVisible(1));
  ASSERT_TRUE(action->GetIsVisible(100));

  action->SetIsVisible(ExtensionAction::kDefaultTabId, false);
  ASSERT_FALSE(action->GetIsVisible(1));
  ASSERT_FALSE(action->GetIsVisible(100));
  action->SetIsVisible(100, true);
  ASSERT_FALSE(action->GetIsVisible(1));
  ASSERT_TRUE(action->GetIsVisible(100));

  action->ClearAllValuesForTab(100);
  ASSERT_FALSE(action->GetIsVisible(1));
  ASSERT_FALSE(action->GetIsVisible(100));

  std::unique_ptr<ExtensionAction> browser_action =
      CreateAction(ActionInfo(ActionInfo::Type::kBrowser));
  ASSERT_TRUE(browser_action->GetIsVisible(1));
}

TEST(ExtensionActionTest, Icon) {
  ActionInfo action_info(ActionInfo::Type::kPage);
  action_info.default_icon.Add(16, "icon16.png");
  std::unique_ptr<ExtensionAction> page_action = CreateAction(action_info);
  ASSERT_TRUE(page_action->default_icon());
  EXPECT_EQ("icon16.png", page_action->default_icon()->Get(
                              16, ExtensionIconSet::Match::kExactly));
  EXPECT_EQ("", page_action->default_icon()->Get(
                    17, ExtensionIconSet::Match::kBigger));
}

TEST(ExtensionActionTest, Badge) {
  std::unique_ptr<ExtensionAction> action =
      CreateAction(ActionInfo(ActionInfo::Type::kPage));
  ASSERT_EQ("", action->GetExplicitlySetBadgeText(1));
  action->SetBadgeText(ExtensionAction::kDefaultTabId, "foo");
  ASSERT_EQ("foo", action->GetExplicitlySetBadgeText(1));
  ASSERT_EQ("foo", action->GetExplicitlySetBadgeText(100));
  action->SetBadgeText(100, "bar");
  ASSERT_EQ("foo", action->GetExplicitlySetBadgeText(1));
  ASSERT_EQ("bar", action->GetExplicitlySetBadgeText(100));
  action->SetBadgeText(ExtensionAction::kDefaultTabId, "baz");
  ASSERT_EQ("baz", action->GetExplicitlySetBadgeText(1));
  action->ClearAllValuesForTab(100);
  ASSERT_EQ("baz", action->GetExplicitlySetBadgeText(100));
}

TEST(ExtensionActionTest, DisplayBadgeText) {
  constexpr int kFirstTabId = 1;
  constexpr int kSecondTabId = 2;

  std::unique_ptr<ExtensionAction> action =
      CreateAction(ActionInfo(ActionInfo::Type::kPage));
  ASSERT_EQ("", action->GetDisplayBadgeText(kFirstTabId));
  action->SetDNRActionCount(kFirstTabId, 10 /* action_count */);
  ASSERT_EQ("10", action->GetDisplayBadgeText(kFirstTabId));
  action->SetBadgeText(ExtensionAction::kDefaultTabId, "foo");
  ASSERT_EQ("10", action->GetDisplayBadgeText(kFirstTabId));
  ASSERT_EQ("foo", action->GetDisplayBadgeText(kSecondTabId));
  action->SetDNRActionCount(kFirstTabId, 0 /* action_count */);
  ASSERT_EQ("foo", action->GetDisplayBadgeText(kFirstTabId));
  action->SetBadgeText(kFirstTabId, "bar");
  ASSERT_EQ("bar", action->GetDisplayBadgeText(kFirstTabId));
  action->SetDNRActionCount(kFirstTabId, 100 /* action_count */);
  ASSERT_EQ("bar", action->GetDisplayBadgeText(kFirstTabId));
  action->ClearAllValuesForTab(kFirstTabId);
  ASSERT_EQ("foo", action->GetDisplayBadgeText(kFirstTabId));
}

TEST(ExtensionActionTest, BadgeTextColor) {
  std::unique_ptr<ExtensionAction> action =
      CreateAction(ActionInfo(ActionInfo::Type::kPage));
  ASSERT_EQ(0x00000000u, action->GetBadgeTextColor(1));
  action->SetBadgeTextColor(ExtensionAction::kDefaultTabId, 0xFFFF0000u);
  ASSERT_EQ(0xFFFF0000u, action->GetBadgeTextColor(1));
  ASSERT_EQ(0xFFFF0000u, action->GetBadgeTextColor(100));
  action->SetBadgeTextColor(100, 0xFF00FF00);
  ASSERT_EQ(0xFFFF0000u, action->GetBadgeTextColor(1));
  ASSERT_EQ(0xFF00FF00u, action->GetBadgeTextColor(100));
  action->SetBadgeTextColor(ExtensionAction::kDefaultTabId, 0xFF0000FFu);
  ASSERT_EQ(0xFF0000FFu, action->GetBadgeTextColor(1));
  action->ClearAllValuesForTab(100);
  ASSERT_EQ(0xFF0000FFu, action->GetBadgeTextColor(100));
}

TEST(ExtensionActionTest, BadgeBackgroundColor) {
  std::unique_ptr<ExtensionAction> action =
      CreateAction(ActionInfo(ActionInfo::Type::kPage));
  ASSERT_EQ(0x00000000u, action->GetBadgeBackgroundColor(1));
  action->SetBadgeBackgroundColor(ExtensionAction::kDefaultTabId, 0xFFFF0000u);
  ASSERT_EQ(0xFFFF0000u, action->GetBadgeBackgroundColor(1));
  ASSERT_EQ(0xFFFF0000u, action->GetBadgeBackgroundColor(100));
  action->SetBadgeBackgroundColor(100, 0xFF00FF00);
  ASSERT_EQ(0xFFFF0000u, action->GetBadgeBackgroundColor(1));
  ASSERT_EQ(0xFF00FF00u, action->GetBadgeBackgroundColor(100));
  action->SetBadgeBackgroundColor(ExtensionAction::kDefaultTabId, 0xFF0000FFu);
  ASSERT_EQ(0xFF0000FFu, action->GetBadgeBackgroundColor(1));
  action->ClearAllValuesForTab(100);
  ASSERT_EQ(0xFF0000FFu, action->GetBadgeBackgroundColor(100));
}

TEST(ExtensionActionTest, PopupUrl) {
  GURL url_unset;
  GURL url_foo("http://www.example.com/foo.html");
  GURL url_bar("http://www.example.com/bar.html");
  GURL url_baz("http://www.example.com/baz.html");

  ActionInfo action_info(ActionInfo::Type::kPage);
  action_info.default_popup_url = url_foo;
  std::unique_ptr<ExtensionAction> action = CreateAction(action_info);

  ASSERT_EQ(url_foo, action->GetPopupUrl(1));
  ASSERT_EQ(url_foo, action->GetPopupUrl(100));
  ASSERT_TRUE(action->HasPopup(1));
  ASSERT_TRUE(action->HasPopup(100));

  action->SetPopupUrl(ExtensionAction::kDefaultTabId, url_unset);
  ASSERT_EQ(url_unset, action->GetPopupUrl(1));
  ASSERT_EQ(url_unset, action->GetPopupUrl(100));
  ASSERT_FALSE(action->HasPopup(1));
  ASSERT_FALSE(action->HasPopup(100));

  action->SetPopupUrl(100, url_bar);
  ASSERT_EQ(url_unset, action->GetPopupUrl(1));
  ASSERT_EQ(url_bar, action->GetPopupUrl(100));

  action->SetPopupUrl(ExtensionAction::kDefaultTabId, url_baz);
  ASSERT_EQ(url_baz, action->GetPopupUrl(1));
  ASSERT_EQ(url_bar, action->GetPopupUrl(100));

  action->ClearAllValuesForTab(100);
  ASSERT_EQ(url_baz, action->GetPopupUrl(1));
  ASSERT_EQ(url_baz, action->GetPopupUrl(100));
}

TEST(ExtensionActiontest, DeclarativeShows) {
  std::unique_ptr<ExtensionAction> action =
      CreateAction(ActionInfo(ActionInfo::Type::kAction));

  // Set the default for the action to be not visible.
  action->SetIsVisible(ExtensionAction::kDefaultTabId, false);
  EXPECT_FALSE(action->GetIsVisible(1));
  EXPECT_FALSE(action->GetIsVisibleIgnoringDeclarative(1));
  EXPECT_FALSE(action->GetIsVisible(100));
  EXPECT_FALSE(action->GetIsVisibleIgnoringDeclarative(100));

  // With a declarative show on a given tab, the action should be visible on
  // that tab only, but shouldn't indicate that if ignoring the declarative
  // values.
  action->DeclarativeShow(1);
  EXPECT_TRUE(action->GetIsVisible(1));
  EXPECT_FALSE(action->GetIsVisibleIgnoringDeclarative(1));
  EXPECT_FALSE(action->GetIsVisible(100));
  EXPECT_FALSE(action->GetIsVisibleIgnoringDeclarative(100));

  // Undo the declarative show. The visibility is reset.
  action->UndoDeclarativeShow(1);
  EXPECT_FALSE(action->GetIsVisible(1));
  EXPECT_FALSE(action->GetIsVisibleIgnoringDeclarative(1));
  EXPECT_FALSE(action->GetIsVisible(100));
  EXPECT_FALSE(action->GetIsVisibleIgnoringDeclarative(100));

  // If there is both a declarative and imperative visibility, it should be
  // considered visible whether or not declarative shows are considered.
  action->DeclarativeShow(1);
  action->SetIsVisible(1, true);
  EXPECT_TRUE(action->GetIsVisible(1));
  EXPECT_TRUE(action->GetIsVisibleIgnoringDeclarative(1));
  EXPECT_FALSE(action->GetIsVisible(100));
  EXPECT_FALSE(action->GetIsVisibleIgnoringDeclarative(100));

  // Similarly, the default can be returned even if we ignore declarative shows.
  action->SetIsVisible(ExtensionAction::kDefaultTabId, true);
  EXPECT_TRUE(action->GetIsVisible(1));
  EXPECT_TRUE(action->GetIsVisibleIgnoringDeclarative(1));
  EXPECT_TRUE(action->GetIsVisible(100));
  EXPECT_TRUE(action->GetIsVisibleIgnoringDeclarative(100));
}

}  // namespace extensions
