// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_mediator.h"

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_drag_session.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_drop_session.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_pinned_tab_collection_consumer.h"
#import "ios/chrome/browser/url_loading/model/fake_url_loading_delegate.h"
#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/test_scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/device_form_factor.h"

namespace {

// Returns a GURL for the given `index`.
GURL GURLWithIndex(int index) {
  return GURL("http://test/url" + base::NumberToString(index));
}

// Returns a FakeDropSession for the given `web_state`.
FakeDropSession* FakeDropSessionWithWebState(web::WebState* web_state) {
  UIDragItem* drag_item = CreateTabDragItem(web_state);
  FakeDropSession* drop_session =
      [[FakeDropSession alloc] initWithItems:@[ drag_item ]];
  return drop_session;
}

}  // namespace

class PinnedTabsMediatorTest : public PlatformTest {
 public:
  PinnedTabsMediatorTest() {
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();

    regular_browser_ = std::make_unique<TestBrowser>(profile_.get());
    incognito_browser_ =
        std::make_unique<TestBrowser>(profile_->GetOffTheRecordProfile());

    browser_list_ = BrowserListFactory::GetForProfile(profile_.get());
    browser_list_->AddBrowser(regular_browser_.get());
    browser_list_->AddBrowser(incognito_browser_.get());

    scene_loader_ = std::make_unique<TestSceneUrlLoadingService>();
    scene_loader_->current_browser_ = regular_browser_.get();
    url_loading_delegate_ = [[FakeURLLoadingDelegate alloc] init];

    // Create loaders, insertion and notifier agents.
    UrlLoadingNotifierBrowserAgent::CreateForBrowser(regular_browser_.get());
    UrlLoadingBrowserAgent::CreateForBrowser(regular_browser_.get());
    TabInsertionBrowserAgent::CreateForBrowser(regular_browser_.get());
    loader_ = UrlLoadingBrowserAgent::FromBrowser(regular_browser_.get());
    loader_->SetSceneService(scene_loader_.get());
    loader_->SetDelegate(url_loading_delegate_);

    // The Pinned Tabs feature is not available on iPad.
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
      consumer_ = [[FakePinnedTabCollectionConsumer alloc] init];
      mediator_ = [[PinnedTabsMediator alloc] initWithConsumer:consumer_];
      mediator_.browser = regular_browser_.get();
    }
  }

  ~PinnedTabsMediatorTest() override {
    // Cleanup to avoid debugger crash in non empty observer lists.
    WebStateList* web_state_list = regular_browser_->GetWebStateList();
    CloseAllWebStates(*web_state_list,
                      WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
    WebStateList* incognito_web_state_list =
        incognito_browser_->GetWebStateList();
    CloseAllWebStates(*incognito_web_state_list,
                      WebStateList::ClosingFlags::CLOSE_NO_FLAGS);
  }

  // Creates a FakeWebState with a navigation history containing exactly only
  // the given `url`.
  std::unique_ptr<web::FakeWebState> CreateFakeWebStateWithURL(
      const GURL& url) {
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->AddItem(url, ui::PAGE_TRANSITION_LINK);
    navigation_manager->SetLastCommittedItem(
        navigation_manager->GetItemAtIndex(0));
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state->SetBrowserState(profile_.get());
    web_state->SetNavigationItemCount(1);
    web_state->SetCurrentURL(url);
    return web_state;
  }

  // Checks that the drag item origin metric is logged in UMA.
  void ExpectThatDragItemOriginMetricLogged(DragItemOrigin origin,
                                            int count = 1) {
    histogram_tester_.ExpectUniqueSample(kUmaPinnedViewDragOrigin, origin,
                                         count);
  }

 protected:
  std::unique_ptr<TestBrowser> regular_browser_;
  std::unique_ptr<Browser> incognito_browser_;
  FakePinnedTabCollectionConsumer* consumer_;
  PinnedTabsMediator* mediator_;
  base::HistogramTester histogram_tester_;

 private:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<BrowserList> browser_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestSceneUrlLoadingService> scene_loader_;
  raw_ptr<UrlLoadingBrowserAgent> loader_;
  FakeURLLoadingDelegate* url_loading_delegate_;
};

// Tests that the consumer is notified when a web state is pinned.
TEST_F(PinnedTabsMediatorTest, ConsumerInsertItem) {
  // The Pinned Tabs feature is not available on iPad.
  if (!IsPinnedTabsEnabled()) {
    return;
  }

  // Inserts two new pinned tabs.
  std::unique_ptr<web::WebState> web_state1 =
      CreateFakeWebStateWithURL(GURLWithIndex(1));
  regular_browser_->GetWebStateList()->InsertWebState(
      std::move(web_state1),
      WebStateList::InsertionParams::Automatic().Pinned());
  std::unique_ptr<web::WebState> web_state2 =
      CreateFakeWebStateWithURL(GURLWithIndex(2));
  regular_browser_->GetWebStateList()->InsertWebState(
      std::move(web_state2),
      WebStateList::InsertionParams::Automatic().Pinned());
  EXPECT_EQ(2UL, consumer_.items.size());

  // Inserts one regular and one incognito tab.
  std::unique_ptr<web::WebState> web_state3 =
      CreateFakeWebStateWithURL(GURLWithIndex(3));
  regular_browser_->GetWebStateList()->InsertWebState(
      std::move(web_state3), WebStateList::InsertionParams::AtIndex(0));
  std::unique_ptr<web::WebState> web_state4 =
      CreateFakeWebStateWithURL(GURLWithIndex(4));
  incognito_browser_->GetWebStateList()->InsertWebState(
      std::move(web_state4), WebStateList::InsertionParams::AtIndex(0));
  EXPECT_EQ(2UL, consumer_.items.size());

  // Inserts a third pinned tab.
  std::unique_ptr<web::WebState> web_state5 =
      CreateFakeWebStateWithURL(GURLWithIndex(5));
  regular_browser_->GetWebStateList()->InsertWebState(
      std::move(web_state5),
      WebStateList::InsertionParams::Automatic().Pinned());
  EXPECT_EQ(3UL, consumer_.items.size());
}

// Tests that the correct UIDropOperation is returned when dropping tabs in the
// pinned view.
TEST_F(PinnedTabsMediatorTest, DropOperation) {
  // The Pinned Tabs feature is not available on iPad.
  if (!IsPinnedTabsEnabled()) {
    return;
  }

  // Tests a regular tab.
  auto regular_web_state = CreateFakeWebStateWithURL(GURLWithIndex(1));
  regular_browser_->GetWebStateList()->InsertWebState(
      std::move(regular_web_state), WebStateList::InsertionParams::AtIndex(0));

  FakeDropSession* regular_drop_session = FakeDropSessionWithWebState(
      regular_browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ([mediator_ dropOperationForDropSession:regular_drop_session
                                           toIndex:0],
            UIDropOperationMove);

  // Tests an incognito tab.
  auto incognito_web_state = CreateFakeWebStateWithURL(GURLWithIndex(2));
  incognito_browser_->GetWebStateList()->InsertWebState(
      std::move(incognito_web_state),
      WebStateList::InsertionParams::AtIndex(0));
  FakeDropSession* incognito_drop_session = FakeDropSessionWithWebState(
      incognito_browser_->GetWebStateList()->GetWebStateAt(0));
  EXPECT_EQ([mediator_ dropOperationForDropSession:incognito_drop_session
                                           toIndex:0],
            UIDropOperationMove);
}

// Tests the drag and drop to reorder webstates.
TEST_F(PinnedTabsMediatorTest, DragAndDropReorder) {
  // The Pinned Tabs feature is not available on iPad.
  if (!IsPinnedTabsEnabled()) {
    return;
  }

  std::unique_ptr<web::WebState> web_state1 =
      CreateFakeWebStateWithURL(GURLWithIndex(1));
  web::WebState* web_state_to_move = web_state1.get();
  regular_browser_->GetWebStateList()->InsertWebState(
      std::move(web_state1),
      WebStateList::InsertionParams::Automatic().Pinned());
  regular_browser_->GetWebStateList()->InsertWebState(
      CreateFakeWebStateWithURL(GURLWithIndex(2)),
      WebStateList::InsertionParams::Automatic().Pinned());
  regular_browser_->GetWebStateList()->InsertWebState(
      CreateFakeWebStateWithURL(GURLWithIndex(3)),
      WebStateList::InsertionParams::Automatic().Pinned());

  ASSERT_EQ(web_state_to_move,
            regular_browser_->GetWebStateList()->GetWebStateAt(0));

  UIDragItem* drag_item = CreateTabDragItem(web_state_to_move);
  [mediator_ dropItem:drag_item toIndex:2 fromSameCollection:YES];

  EXPECT_EQ(web_state_to_move,
            regular_browser_->GetWebStateList()->GetWebStateAt(2));
}

// Tests dropping pinned tabs.
TEST_F(PinnedTabsMediatorTest, DropPinnedTabs) {
  // The Pinned Tabs feature is not available on iPad.
  if (!IsPinnedTabsEnabled()) {
    return;
  }

  WebStateList* web_state_list = regular_browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a* b c | d e f", regular_browser_->GetProfile()));

  // Drop "A" after "C".
  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(0)->GetUniqueIdentifier();
  id local_object =
      [[TabInfo alloc] initWithTabID:web_state_id
                             profile:regular_browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:2 fromSameCollection:YES];
  EXPECT_EQ("b c a* | d e f", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameCollection, 1);

  // Drop "C" before "B".
  web_state_id = web_state_list->GetWebStateAt(1)->GetUniqueIdentifier();
  local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                        profile:regular_browser_->GetProfile()];
  item_provider = [[NSItemProvider alloc] init];
  drag_item = [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:0 fromSameCollection:YES];
  EXPECT_EQ("c b a* | d e f", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameCollection, 2);
}

// Tests dropping regular tabs .
TEST_F(PinnedTabsMediatorTest, DropRegularTabs) {
  // The Pinned Tabs feature is not available on iPad.
  if (!IsPinnedTabsEnabled()) {
    return;
  }

  WebStateList* web_state_list = regular_browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a* b c | d e f", regular_browser_->GetProfile()));

  // Drop "E" after "C".
  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(4)->GetUniqueIdentifier();
  id local_object =
      [[TabInfo alloc] initWithTabID:web_state_id
                             profile:regular_browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:3 fromSameCollection:NO];
  EXPECT_EQ("a* b c e | d f", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameBrowser, 1);

  // Drop "D" after "E".
  web_state_id = web_state_list->GetWebStateAt(4)->GetUniqueIdentifier();
  local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                        profile:regular_browser_->GetProfile()];
  item_provider = [[NSItemProvider alloc] init];
  drag_item = [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:4 fromSameCollection:NO];
  EXPECT_EQ("a* b c e d | f", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameBrowser, 2);
}

// Tests dropping tabs from tab group.
TEST_F(PinnedTabsMediatorTest, DropTabGroupTabs) {
  // The Pinned Tabs feature is not available on iPad.
  if (!IsPinnedTabsEnabled()) {
    return;
  }

  WebStateList* web_state_list = regular_browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a* b c | d [ 0 e f ]", regular_browser_->GetProfile()));

  // Drop "E" after "C".
  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(4)->GetUniqueIdentifier();
  id local_object =
      [[TabInfo alloc] initWithTabID:web_state_id
                             profile:regular_browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:3 fromSameCollection:NO];
  EXPECT_EQ("a* b c e | d [ 0 f ]", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameBrowser, 1);

  // Drop "D" after "E".
  web_state_id = web_state_list->GetWebStateAt(4)->GetUniqueIdentifier();
  local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                        profile:regular_browser_->GetProfile()];
  item_provider = [[NSItemProvider alloc] init];
  drag_item = [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:4 fromSameCollection:NO];
  EXPECT_EQ("a* b c e d | [ 0 f ]", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameBrowser, 2);

  // Drop "F" after "D".
  web_state_id = web_state_list->GetWebStateAt(5)->GetUniqueIdentifier();
  local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                        profile:regular_browser_->GetProfile()];
  item_provider = [[NSItemProvider alloc] init];
  drag_item = [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:5 fromSameCollection:NO];
  EXPECT_EQ("a* b c e d f |", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameBrowser, 3);
}

// Tests dropping an external URL.
TEST_F(PinnedTabsMediatorTest, DropExternalURL) {
  // The Pinned Tabs feature is not available on iPad.
  if (!IsPinnedTabsEnabled()) {
    return;
  }

  WebStateList* web_state_list = regular_browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a* b c | d", regular_browser_->GetProfile()));
  ASSERT_EQ(4, web_state_list->count());

  NSItemProvider* item_provider = [[NSItemProvider alloc]
      initWithContentsOfURL:[NSURL URLWithString:@"https://dragged_url.com"]];

  // Drop item after "C".
  [mediator_ dropItemFromProvider:item_provider
                          toIndex:3
               placeholderContext:nil];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Seconds(1), ^bool(void) {
        return web_state_list->count() == 5;
      }));
  web::WebState* web_state = web_state_list->GetWebStateAt(3);
  EXPECT_EQ(GURL("https://dragged_url.com"),
            web_state->GetNavigationManager()->GetPendingItem()->GetURL());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOther);
}
