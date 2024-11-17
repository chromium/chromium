// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_mediator_utils.h"

#import "base/test/scoped_feature_list.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_local_update_observer.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

namespace {

// Creates a MockTabGroupSyncService.
std::unique_ptr<KeyedService> CreateMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<
      testing::NiceMock<tab_groups::MockTabGroupSyncService>>();
}

// Creates a WebState for test purposes.
std::unique_ptr<web::WebState> CreateWebState() {
  auto web_state =
      std::make_unique<web::FakeWebState>(web::WebStateID::NewUnique());
  SnapshotTabHelper::CreateForWebState(web_state.get());
  return std::move(web_state);
}

}  // namespace

// Test fixture for the TabStripMediator.
class TabStripMediatorUtilsTest : public PlatformTest {
 public:
  TabStripMediatorUtilsTest() {
    feature_list_.InitWithFeatures(
        {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});
    TestProfileIOS::Builder profile_builder;
    profile_builder.AddTestingFactory(
        tab_groups::TabGroupSyncServiceFactory::GetInstance(),
        base::BindRepeating(&CreateMockSyncService));
    profile_ = std::move(profile_builder).Build();
    mock_service_ = static_cast<tab_groups::MockTabGroupSyncService*>(
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile_.get()));
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<FakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();
    other_browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<FakeWebStateListDelegate>());
    other_web_state_list_ = other_browser_->GetWebStateList();
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    local_observer_ = std::make_unique<tab_groups::TabGroupLocalUpdateObserver>(
        browser_list, mock_service_);
    browser_list->AddBrowser(browser_.get());
    browser_list->AddBrowser(other_browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(other_browser_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  std::unique_ptr<TestBrowser> other_browser_;
  raw_ptr<WebStateList> other_web_state_list_;
  raw_ptr<tab_groups::MockTabGroupSyncService> mock_service_;
  std::unique_ptr<tab_groups::TabGroupLocalUpdateObserver> local_observer_;
};

// Test that `CreateTabItemIdentifier` returns the correct
// `TabStripItemIdentifier` for a WebState.
TEST_F(TabStripMediatorUtilsTest, CreateTabItemIdentifier) {
  WebStateListBuilderFromDescription builder(web_state_list_.get());
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| [ 0 a b* ] c [ 1 d e ]", base::BindRepeating(CreateWebState)));

  web::WebState* webstate_a = builder.GetWebStateForIdentifier('a');
  web::WebState* webstate_b = builder.GetWebStateForIdentifier('b');
  web::WebState* webstate_c = builder.GetWebStateForIdentifier('c');
  web::WebState* webstate_d = builder.GetWebStateForIdentifier('d');
  web::WebState* webstate_e = builder.GetWebStateForIdentifier('e');

  TabStripItemIdentifier* webstate_a_item_identifier =
      CreateTabItemIdentifier(webstate_a);
  EXPECT_EQ(webstate_a->GetUniqueIdentifier(),
            webstate_a_item_identifier.tabSwitcherItem.identifier);
  TabStripItemIdentifier* webstate_b_item_identifier =
      CreateTabItemIdentifier(webstate_b);
  EXPECT_EQ(webstate_b->GetUniqueIdentifier(),
            webstate_b_item_identifier.tabSwitcherItem.identifier);
  TabStripItemIdentifier* webstate_c_item_identifier =
      CreateTabItemIdentifier(webstate_c);
  EXPECT_EQ(webstate_c->GetUniqueIdentifier(),
            webstate_c_item_identifier.tabSwitcherItem.identifier);
  TabStripItemIdentifier* webstate_d_item_identifier =
      CreateTabItemIdentifier(webstate_d);
  EXPECT_EQ(webstate_d->GetUniqueIdentifier(),
            webstate_d_item_identifier.tabSwitcherItem.identifier);
  TabStripItemIdentifier* webstate_e_item_identifier =
      CreateTabItemIdentifier(webstate_e);
  EXPECT_EQ(webstate_e->GetUniqueIdentifier(),
            webstate_e_item_identifier.tabSwitcherItem.identifier);
}

// Test that `CreateGroupItemIdentifier` returns the correct
// `TabStripItemIdentifier` for a TabGroup.
TEST_F(TabStripMediatorUtilsTest, CreateGroupItemIdentifier) {
  WebStateListBuilderFromDescription builder(web_state_list_.get());
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| [ 0 a b* ] c [ 1 d e ]", base::BindRepeating(CreateWebState)));

  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  TabStripItemIdentifier* group_0_item_identifier =
      CreateGroupItemIdentifier(group_0, web_state_list_.get());
  EXPECT_EQ(group_0, group_0_item_identifier.tabGroupItem.tabGroup);
  TabStripItemIdentifier* group_1_item_identifier =
      CreateGroupItemIdentifier(group_1, web_state_list_.get());
  EXPECT_EQ(group_1, group_1_item_identifier.tabGroupItem.tabGroup);
}

// Test that calling `MoveGroupBeforeTabStripItem` in the same browser works as
// expected.
TEST_F(TabStripMediatorUtilsTest, MoveGroupBeforeItemSameBrowser) {
  WebStateListBuilderFromDescription builder(web_state_list_.get());
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| [ 0 a b* ] c [ 1 d e ]", base::BindRepeating(CreateWebState)));

  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  web::WebState* webstate_a = builder.GetWebStateForIdentifier('a');
  web::WebState* webstate_b = builder.GetWebStateForIdentifier('b');
  web::WebState* webstate_c = builder.GetWebStateForIdentifier('c');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');
  web::WebState* webstate_d = builder.GetWebStateForIdentifier('d');
  web::WebState* webstate_e = builder.GetWebStateForIdentifier('e');

  TabStripItemIdentifier* group_0_item_identifier =
      CreateGroupItemIdentifier(group_0, web_state_list_.get());
  TabStripItemIdentifier* webstate_a_item_identifier =
      CreateTabItemIdentifier(webstate_a);
  TabStripItemIdentifier* webstate_b_item_identifier =
      CreateTabItemIdentifier(webstate_b);
  TabStripItemIdentifier* webstate_c_item_identifier =
      CreateTabItemIdentifier(webstate_c);
  TabStripItemIdentifier* group_1_item_identifier =
      CreateGroupItemIdentifier(group_1, web_state_list_.get());
  TabStripItemIdentifier* webstate_d_item_identifier =
      CreateTabItemIdentifier(webstate_d);
  TabStripItemIdentifier* webstate_e_item_identifier =
      CreateTabItemIdentifier(webstate_e);

  EXPECT_EQ("| [ 0 a b* ] c [ 1 d e ]", builder.GetWebStateListDescription());

  // Move `group_1` before `webstate_c_item_identifier`.
  MoveGroupBeforeTabStripItem(group_1, webstate_c_item_identifier,
                              browser_.get());
  EXPECT_EQ("| [ 0 a b* ] [ 1 d e ] c", builder.GetWebStateListDescription());

  // Move `group_1` at the end of the WebStateList.
  MoveGroupBeforeTabStripItem(group_1, nil, browser_.get());
  EXPECT_EQ("| [ 0 a b* ] c [ 1 d e ]", builder.GetWebStateListDescription());

  // Move `group_1` before `webstate_b_item_identifier`.
  MoveGroupBeforeTabStripItem(group_1, webstate_b_item_identifier,
                              browser_.get());
  EXPECT_EQ("| [ 1 d e ] [ 0 a b* ] c", builder.GetWebStateListDescription());

  // Move `group_0` at the end of the WebStateList.
  MoveGroupBeforeTabStripItem(group_0, nil, browser_.get());
  EXPECT_EQ("| [ 1 d e ] c [ 0 a b* ]", builder.GetWebStateListDescription());

  // Move `group_0` before `group_1_item_identifier`.
  MoveGroupBeforeTabStripItem(group_0, group_1_item_identifier, browser_.get());
  EXPECT_EQ("| [ 0 a b* ] [ 1 d e ] c", builder.GetWebStateListDescription());

  // Move `group_1` before `group_0_item_identifier`.
  MoveGroupBeforeTabStripItem(group_1, group_0_item_identifier, browser_.get());
  EXPECT_EQ("| [ 1 d e ] [ 0 a b* ] c", builder.GetWebStateListDescription());

  // Move `group_0` before `webstate_e_item_identifier`.
  MoveGroupBeforeTabStripItem(group_0, webstate_e_item_identifier,
                              browser_.get());
  EXPECT_EQ("| [ 0 a b* ] [ 1 d e ] c", builder.GetWebStateListDescription());

  // Move `group_1` before `webstate_a_item_identifier`.
  MoveGroupBeforeTabStripItem(group_1, webstate_a_item_identifier,
                              browser_.get());
  EXPECT_EQ("| [ 1 d e ] [ 0 a b* ] c", builder.GetWebStateListDescription());

  // Move `group_1` before `webstate_d_item_identifier`.
  MoveGroupBeforeTabStripItem(group_1, webstate_d_item_identifier,
                              browser_.get());
  EXPECT_EQ("| [ 1 d e ] [ 0 a b* ] c", builder.GetWebStateListDescription());
}

// Test that calling `MoveGroupBeforeTabStripItem` between browsers works as
// expected.
TEST_F(TabStripMediatorUtilsTest, MoveGroupBeforeItemDifferentBrowser) {
  WebStateListBuilderFromDescription builder(web_state_list_.get());
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| [ 0 a b* ] c", base::BindRepeating(CreateWebState)));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  web::WebState* webstate_a = builder.GetWebStateForIdentifier('a');
  web::WebState* webstate_b = builder.GetWebStateForIdentifier('b');
  web::WebState* webstate_c = builder.GetWebStateForIdentifier('c');
  TabStripItemIdentifier* webstate_c_item_identifier =
      CreateTabItemIdentifier(webstate_c);

  WebStateListBuilderFromDescription other_builder(other_web_state_list_);
  ASSERT_TRUE(other_builder.BuildWebStateListFromDescription(
      "| d [ 1 e f ]", base::BindRepeating(CreateWebState)));
  web::WebState* webstate_d = other_builder.GetWebStateForIdentifier('d');
  const TabGroup* group_1 = other_builder.GetTabGroupForIdentifier('1');
  web::WebState* webstate_e = other_builder.GetWebStateForIdentifier('e');
  web::WebState* webstate_f = other_builder.GetWebStateForIdentifier('f');
  TabStripItemIdentifier* webstate_d_item_identifier =
      CreateTabItemIdentifier(webstate_d);
  TabStripItemIdentifier* webstate_f_item_identifier =
      CreateTabItemIdentifier(webstate_f);

  EXPECT_EQ("| [ 0 a b* ] c", builder.GetWebStateListDescription());
  EXPECT_EQ("| d [ 1 e f ]", other_builder.GetWebStateListDescription());
  EXPECT_CALL(*mock_service_, CreateScopedLocalObserverPauser()).Times(5);
  local_observer_->SetSyncUpdatePaused(true);
  EXPECT_CALL(*mock_service_, RemoveGroup(group_0->tab_group_id())).Times(0);

  // Move `group_0` before `webstate_f_item_identifier` in `other_browser_`.
  MoveGroupBeforeTabStripItem(group_0, webstate_f_item_identifier,
                              other_browser_.get());
  other_builder.SetWebStateIdentifier(webstate_a, 'a');
  other_builder.SetWebStateIdentifier(webstate_b, 'b');
  other_builder.GenerateIdentifiersForWebStateList();
  group_0 = other_builder.GetTabGroupForIdentifier('0');
  EXPECT_EQ("| c*", builder.GetWebStateListDescription());
  EXPECT_EQ("| d [ 0 a b ] [ 1 e f ]",
            other_builder.GetWebStateListDescription());

  // Move `group_0` at the end of the WebStateList in `browser_`.
  MoveGroupBeforeTabStripItem(group_0, nil, browser_.get());
  builder.SetWebStateIdentifier(webstate_a, 'a');
  builder.SetWebStateIdentifier(webstate_b, 'b');
  builder.GenerateIdentifiersForWebStateList();
  group_0 = builder.GetTabGroupForIdentifier('0');
  EXPECT_EQ("| c* [ 0 a b ]", builder.GetWebStateListDescription());
  EXPECT_EQ("| d [ 1 e f ]", other_builder.GetWebStateListDescription());

  // Move `group_1` before `webstate_c_item_identifier` in `browser_`.
  MoveGroupBeforeTabStripItem(group_1, webstate_c_item_identifier,
                              browser_.get());
  builder.SetWebStateIdentifier(webstate_e, 'e');
  builder.SetWebStateIdentifier(webstate_f, 'f');
  builder.GenerateIdentifiersForWebStateList();
  group_1 = builder.GetTabGroupForIdentifier('1');
  EXPECT_EQ("| [ 1 e f ] c* [ 0 a b ]", builder.GetWebStateListDescription());
  EXPECT_EQ("| d", other_builder.GetWebStateListDescription());

  // Move `group_1` before `webstate_d_item_identifier` in `other_browser_`.
  MoveGroupBeforeTabStripItem(group_1, webstate_d_item_identifier,
                              other_browser_.get());
  other_builder.SetWebStateIdentifier(webstate_e, 'e');
  other_builder.SetWebStateIdentifier(webstate_f, 'f');
  group_1 = other_web_state_list_->GetGroupOfWebStateAt(
      other_web_state_list_->GetIndexOfWebState(webstate_e));
  ASSERT_NE(nullptr, group_1);
  other_builder.SetTabGroupIdentifier(group_1, '1');
  EXPECT_EQ("| c* [ 0 a b ]", builder.GetWebStateListDescription());
  EXPECT_EQ("| [ 1 e f ] d", other_builder.GetWebStateListDescription());

  // Move `group_1` before `group_0_item_identifier` in `browser_`.
  MoveGroupBeforeTabStripItem(
      group_1, CreateGroupItemIdentifier(group_0, web_state_list_.get()),
      browser_.get());
  builder.SetWebStateIdentifier(webstate_e, 'e');
  builder.SetWebStateIdentifier(webstate_f, 'f');
  builder.GenerateIdentifiersForWebStateList();
  group_1 = builder.GetTabGroupForIdentifier('1');
  EXPECT_EQ("| c* [ 1 e f ] [ 0 a b ]", builder.GetWebStateListDescription());
  EXPECT_EQ("| d", other_builder.GetWebStateListDescription());
}
