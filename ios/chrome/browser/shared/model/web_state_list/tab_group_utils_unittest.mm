// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"

#import "components/favicon/core/favicon_service.h"
#import "components/favicon/core/favicon_url.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using tab_groups::TabGroupId;

class GroupUtilsTest : public PlatformTest {
 public:
  GroupUtilsTest() {
    TestProfileIOS::Builder profile_builder;
    profile_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());

    profile_ = std::move(profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    other_browser_ = std::make_unique<TestBrowser>(profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());
    other_incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());

    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(other_browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(incognito_browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(other_incognito_browser_.get());

    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    browser_list_->AddBrowser(browser_.get());
    browser_list_->AddBrowser(incognito_browser_.get());

    web_state_list_ = browser_->GetWebStateList();
    incognito_web_state_list_ = incognito_browser_->GetWebStateList();
    other_web_state_list_ = other_browser_->GetWebStateList();
    other_incognito_web_state_list_ =
        other_incognito_browser_->GetWebStateList();
  }

  // Adds a new web state to `web_state_list`.
  void AddWebStateToList(WebStateList* web_state_list) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    SnapshotTabHelper::CreateForWebState(web_state.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state.get(),
        ios::FaviconServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::IMPLICIT_ACCESS));

    web_state_list->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  // Adds a web state to the default regular web state list.
  void AddWebState() { AddWebStateToList(web_state_list_); }

  // Adds a web state to the default incognito web state list.
  void AddIncognitoWebState() { AddWebStateToList(incognito_web_state_list_); }

  // Adds several web states to all web state lists.
  void AddDefaultWebStates() {
    AddWebState();
    AddWebState();
    AddWebState();
    AddIncognitoWebState();
    AddIncognitoWebState();
    AddIncognitoWebState();
    AddIncognitoWebState();

    AddWebStateToList(other_web_state_list_);
    AddWebStateToList(other_web_state_list_);
    AddWebStateToList(other_incognito_web_state_list_);
    AddWebStateToList(other_incognito_web_state_list_);
    AddWebStateToList(other_incognito_web_state_list_);
  }

  // Creates a new group in the default regular web state list containing the
  // web state at `web_state_index` with a default title and a `color`.
  void CreateGroup(int web_state_index, tab_groups::TabGroupColorId color) {
    tab_groups::TabGroupVisualData visual_data(u"Test title", color);
    web_state_list_->CreateGroup({web_state_index}, visual_data,
                                 TabGroupId::GenerateNew());
  }

  // Returns the default color for the regular web state list.
  tab_groups::TabGroupColorId DefaultColor() {
    return TabGroup::DefaultColorForNewTabGroup(web_state_list_.get());
  }

  // Adds the other browsers to the browser list.
  void AddOtherBrowsers() {
    browser_list_->AddBrowser(other_browser_.get());
    browser_list_->AddBrowser(other_incognito_browser_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestBrowser> other_browser_;
  std::unique_ptr<TestBrowser> incognito_browser_;
  std::unique_ptr<TestBrowser> other_incognito_browser_;
  raw_ptr<BrowserList> browser_list_;
  raw_ptr<WebStateList> web_state_list_;
  raw_ptr<WebStateList> other_web_state_list_;
  raw_ptr<WebStateList> incognito_web_state_list_;
  raw_ptr<WebStateList> other_incognito_web_state_list_;
};

TEST_F(GroupUtilsTest, TestDefaultColor) {
  std::vector<tab_groups::TabGroupColorId> colors =
  TabGroup::AllPossibleTabGroupColors();

  for (unsigned int i = 0; i < colors.size() * 3 + 1; i++) {
    AddWebState();
  }

  EXPECT_EQ(colors[0], DefaultColor());

  // Check the first two independently to make sure that even if the first web
  // state is not in a group it is still working.
  CreateGroup(1, colors[0]);
  EXPECT_EQ(colors[1], DefaultColor());

  CreateGroup(0, colors[1]);
  EXPECT_EQ(colors[2], DefaultColor());

  // Check the following colors.
  for (unsigned int i = 2; i < colors.size() - 1; i++) {
    CreateGroup(i, colors[i]);
    EXPECT_EQ(colors[i + 1], DefaultColor());
  }

  // Check the last one indenpendently. It should cycle.
  CreateGroup(colors.size() - 1, colors[colors.size() - 1]);
  EXPECT_EQ(colors[0], DefaultColor());

  // Cycle again.
  CreateGroup(colors.size(), colors[0]);
  EXPECT_EQ(colors[1], DefaultColor());
}

// Tests getting all the groups if the app only contains one window.
TEST_F(GroupUtilsTest, AllGroupsSingleWindow) {
  AddDefaultWebStates();

  TabGroupId tab_group_id_1 = TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data1(
      u"First title", tab_groups::TabGroupColorId::kGreen);
  web_state_list_->CreateGroup({0}, visual_data1, tab_group_id_1);

  TabGroupId tab_group_id_2 = TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data2(
      u"Second title", tab_groups::TabGroupColorId::kPink);
  web_state_list_->CreateGroup({1}, visual_data2, tab_group_id_2);

  TabGroupId tab_group_id_3 = TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data3(
      u"Third title", tab_groups::TabGroupColorId::kCyan);
  incognito_web_state_list_->CreateGroup({3}, visual_data3, tab_group_id_3);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  const bool incognito = profile_->IsOffTheRecord();
  std::set<const TabGroup*> groups =
      GetAllGroupsForBrowserList(browser_list, incognito);
  EXPECT_EQ(groups, GetAllGroupsForProfile(profile_.get()));

  std::vector<TabGroupId> tab_group_ids;
  std::vector<tab_groups::TabGroupVisualData> visual_data;
  for (const TabGroup* group : groups) {
    tab_group_ids.push_back(group->tab_group_id());
    visual_data.push_back(group->visual_data());
  }

  EXPECT_EQ(2u, groups.size());
  EXPECT_TRUE(std::find(visual_data.begin(), visual_data.end(), visual_data1) !=
              visual_data.end());
  EXPECT_TRUE(std::find(visual_data.begin(), visual_data.end(), visual_data2) !=
              visual_data.end());
  EXPECT_TRUE(std::find(tab_group_ids.begin(), tab_group_ids.end(),
                        tab_group_id_1) != tab_group_ids.end());
  EXPECT_TRUE(std::find(tab_group_ids.begin(), tab_group_ids.end(),
                        tab_group_id_2) != tab_group_ids.end());

  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  BrowserList* incognito_browser_list =
      BrowserListFactory::GetForProfile(otr_profile);
  std::set<const TabGroup*> incognito_groups =
      GetAllGroupsForBrowserList(incognito_browser_list, true);
  EXPECT_EQ(incognito_groups, GetAllGroupsForProfile(otr_profile));

  EXPECT_EQ(1u, incognito_groups.size());
  for (const TabGroup* group : incognito_groups) {
    EXPECT_EQ(tab_group_id_3, group->tab_group_id());
    EXPECT_EQ(visual_data3, group->visual_data());
  }
}

// Tests getting all the groups if the app contains two window.
TEST_F(GroupUtilsTest, AllGroupsMultipleWindows) {
  AddOtherBrowsers();
  AddDefaultWebStates();

  TabGroupId tab_group_id_1 = TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data1(
      u"First title", tab_groups::TabGroupColorId::kGreen);
  web_state_list_->CreateGroup({0}, visual_data1, tab_group_id_1);

  TabGroupId tab_group_id_2 = TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data2(
      u"Second title", tab_groups::TabGroupColorId::kPink);
  web_state_list_->CreateGroup({1}, visual_data2, tab_group_id_2);

  TabGroupId tab_group_id_3 = TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data3(
      u"Third title", tab_groups::TabGroupColorId::kCyan);
  incognito_web_state_list_->CreateGroup({3}, visual_data3, tab_group_id_3);

  TabGroupId tab_group_id_4 = TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data4(
      u"Fourth title", tab_groups::TabGroupColorId::kPurple);
  other_web_state_list_->CreateGroup({1}, visual_data4, tab_group_id_4);

  TabGroupId tab_group_id_5 = TabGroupId::GenerateNew();
  tab_groups::TabGroupVisualData visual_data5(
      u"Fifth title", tab_groups::TabGroupColorId::kYellow);
  other_web_state_list_->CreateGroup({0}, visual_data5, tab_group_id_5);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());
  const bool incognito = profile_->IsOffTheRecord();
  std::set<const TabGroup*> groups =
      GetAllGroupsForBrowserList(browser_list, incognito);
  EXPECT_EQ(groups, GetAllGroupsForProfile(profile_.get()));

  std::vector<TabGroupId> tab_group_ids;
  std::vector<tab_groups::TabGroupVisualData> visual_data;
  for (const TabGroup* group : groups) {
    tab_group_ids.push_back(group->tab_group_id());
    visual_data.push_back(group->visual_data());
  }

  EXPECT_EQ(4u, groups.size());
  EXPECT_TRUE(std::find(visual_data.begin(), visual_data.end(), visual_data1) !=
              visual_data.end());
  EXPECT_TRUE(std::find(visual_data.begin(), visual_data.end(), visual_data2) !=
              visual_data.end());
  EXPECT_TRUE(std::find(visual_data.begin(), visual_data.end(), visual_data4) !=
              visual_data.end());
  EXPECT_TRUE(std::find(visual_data.begin(), visual_data.end(), visual_data5) !=
              visual_data.end());
  EXPECT_TRUE(std::find(tab_group_ids.begin(), tab_group_ids.end(),
                        tab_group_id_1) != tab_group_ids.end());
  EXPECT_TRUE(std::find(tab_group_ids.begin(), tab_group_ids.end(),
                        tab_group_id_2) != tab_group_ids.end());
  EXPECT_TRUE(std::find(tab_group_ids.begin(), tab_group_ids.end(),
                        tab_group_id_4) != tab_group_ids.end());
  EXPECT_TRUE(std::find(tab_group_ids.begin(), tab_group_ids.end(),
                        tab_group_id_5) != tab_group_ids.end());

  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  BrowserList* incognito_browser_list =
      BrowserListFactory::GetForProfile(otr_profile);
  std::set<const TabGroup*> incognito_groups =
      GetAllGroupsForBrowserList(incognito_browser_list, true);
  EXPECT_EQ(incognito_groups, GetAllGroupsForProfile(otr_profile));

  EXPECT_EQ(1u, incognito_groups.size());
  for (const TabGroup* group : incognito_groups) {
    EXPECT_EQ(tab_group_id_3, group->tab_group_id());
    EXPECT_EQ(visual_data3, group->visual_data());
  }
}

// Tests getting all the groups if the app only contains one window.
TEST_F(GroupUtilsTest, MoveToGroupSingleWindow) {
  AddDefaultWebStates();

  tab_groups::TabGroupVisualData visual_data(
      u"First title", tab_groups::TabGroupColorId::kGreen);
  web_state_list_->CreateGroup({1}, visual_data, TabGroupId::GenerateNew());

  web::WebStateID web_state_id =
      web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier();

  ASSERT_EQ(nullptr, web_state_list_->GetGroupOfWebStateAt(0));

  const TabGroup* destination_group = web_state_list_->GetGroupOfWebStateAt(1);
  MoveTabToGroup(web_state_id, destination_group, profile_.get());

  int new_index = GetWebStateIndex(
      web_state_list_, WebStateSearchCriteria{.identifier = web_state_id});

  EXPECT_EQ(destination_group,
            web_state_list_->GetGroupOfWebStateAt(new_index));
  // The web state should have been moved to the end of the group.
  EXPECT_EQ(1, new_index);

  // Trying to move an incognito web state to a non-incognito group should do
  // nothing.
  web::WebStateID incognito_web_state_id =
      incognito_web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier();

  ASSERT_EQ(nullptr, incognito_web_state_list_->GetGroupOfWebStateAt(1));

  MoveTabToGroup(incognito_web_state_id, destination_group,
                 profile_->GetOffTheRecordProfile());

  EXPECT_EQ(nullptr, incognito_web_state_list_->GetGroupOfWebStateAt(1));
}

// Tests getting all the groups if the app contains two window.
TEST_F(GroupUtilsTest, MoveToGroupMultipleWindow) {
  AddOtherBrowsers();
  AddDefaultWebStates();

  tab_groups::TabGroupVisualData visual_data(
      u"First title", tab_groups::TabGroupColorId::kGreen);
  other_web_state_list_->CreateGroup({0}, visual_data,
                                     TabGroupId::GenerateNew());

  web::WebStateID web_state_id =
      web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier();

  ASSERT_EQ(nullptr, web_state_list_->GetGroupOfWebStateAt(1));
  ASSERT_EQ(3, web_state_list_->count());
  ASSERT_EQ(2, other_web_state_list_->count());

  const TabGroup* destination_group =
      other_web_state_list_->GetGroupOfWebStateAt(0);
  MoveTabToGroup(web_state_id, destination_group, profile_.get());

  // The web state is removed from the original list.
  EXPECT_EQ(2, web_state_list_->count());
  int index_in_original_list = GetWebStateIndex(
      web_state_list_, WebStateSearchCriteria{.identifier = web_state_id});
  EXPECT_EQ(WebStateList::kInvalidIndex, index_in_original_list);

  // It is added with the right group in the other list.
  int index_in_other_list =
      GetWebStateIndex(other_web_state_list_,
                       WebStateSearchCriteria{.identifier = web_state_id});
  EXPECT_EQ(1, index_in_other_list);
  EXPECT_EQ(3, other_web_state_list_->count());
  EXPECT_EQ(destination_group,
            other_web_state_list_->GetGroupOfWebStateAt(index_in_other_list));
}

// Tests finding the Browser with a group in its WebStateList, with a single
// window.
TEST_F(GroupUtilsTest, GetBrowserForGroupSingleWindow) {
  AddDefaultWebStates();

  tab_groups::TabGroupVisualData visual_data1(
      u"First title", tab_groups::TabGroupColorId::kGreen);
  const TabGroup* group1 = web_state_list_->CreateGroup(
      {0}, visual_data1, TabGroupId::GenerateNew());

  tab_groups::TabGroupVisualData visual_data2(
      u"Second title", tab_groups::TabGroupColorId::kPink);
  const TabGroup* group2 = web_state_list_->CreateGroup(
      {1}, visual_data2, TabGroupId::GenerateNew());

  tab_groups::TabGroupVisualData visual_data3(
      u"Third title", tab_groups::TabGroupColorId::kCyan);
  const TabGroup* incognito_group3 = incognito_web_state_list_->CreateGroup(
      {3}, visual_data3, TabGroupId::GenerateNew());

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());

  // Looking in the correct location should return the expected browser.
  EXPECT_EQ(browser_.get(), GetBrowserForGroup(browser_list, group1, false));
  EXPECT_EQ(browser_.get(), GetBrowserForGroup(browser_list, group2, false));
  EXPECT_EQ(incognito_browser_.get(),
            GetBrowserForGroup(browser_list, incognito_group3, true));

  // Looking in the wrong location e.g. Incognito group inside regular browsers
  // should return `nullptr`.
  EXPECT_EQ(nullptr, GetBrowserForGroup(browser_list, group1, true));
  EXPECT_EQ(nullptr, GetBrowserForGroup(browser_list, group2, true));
  EXPECT_EQ(nullptr, GetBrowserForGroup(browser_list, incognito_group3, false));
}

// Tests finding the Browser with a group in its WebStateList, with a multiple
// windows.
TEST_F(GroupUtilsTest, GetBrowserForGroupMultipleWindows) {
  AddOtherBrowsers();
  AddDefaultWebStates();

  tab_groups::TabGroupVisualData visual_data1(
      u"First title", tab_groups::TabGroupColorId::kGreen);
  const TabGroup* group1 = web_state_list_->CreateGroup(
      {0}, visual_data1, TabGroupId::GenerateNew());

  tab_groups::TabGroupVisualData visual_data2(
      u"Second title", tab_groups::TabGroupColorId::kPink);
  const TabGroup* group2 = web_state_list_->CreateGroup(
      {1}, visual_data2, TabGroupId::GenerateNew());

  tab_groups::TabGroupVisualData visual_data3(
      u"Third title", tab_groups::TabGroupColorId::kCyan);
  const TabGroup* incognito_group3 = incognito_web_state_list_->CreateGroup(
      {3}, visual_data3, TabGroupId::GenerateNew());

  tab_groups::TabGroupVisualData visual_data4(
      u"Fourth title", tab_groups::TabGroupColorId::kPurple);
  const TabGroup* other_group4 = other_web_state_list_->CreateGroup(
      {1}, visual_data4, TabGroupId::GenerateNew());

  tab_groups::TabGroupVisualData visual_data5(
      u"Fifth title", tab_groups::TabGroupColorId::kYellow);
  const TabGroup* other_group5 = other_web_state_list_->CreateGroup(
      {0}, visual_data5, TabGroupId::GenerateNew());

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_.get());

  // Looking in the correct location should return the expected browser.
  EXPECT_EQ(browser_.get(), GetBrowserForGroup(browser_list, group1, false));
  EXPECT_EQ(browser_.get(), GetBrowserForGroup(browser_list, group2, false));
  EXPECT_EQ(incognito_browser_.get(),
            GetBrowserForGroup(browser_list, incognito_group3, true));
  EXPECT_EQ(other_browser_.get(),
            GetBrowserForGroup(browser_list, other_group4, false));
  EXPECT_EQ(other_browser_.get(),
            GetBrowserForGroup(browser_list, other_group5, false));

  // Looking in the wrong location e.g. Incognito group inside regular browsers
  // should return `nullptr`.
  EXPECT_EQ(nullptr, GetBrowserForGroup(browser_list, group1, true));
  EXPECT_EQ(nullptr, GetBrowserForGroup(browser_list, group2, true));
  EXPECT_EQ(nullptr, GetBrowserForGroup(browser_list, incognito_group3, false));
  EXPECT_EQ(nullptr, GetBrowserForGroup(browser_list, other_group4, true));
  EXPECT_EQ(nullptr, GetBrowserForGroup(browser_list, other_group5, true));
}
