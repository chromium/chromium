// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/open_tabs_spotlight_manager.h"

#import "base/apple/foundation_util.h"
#import "base/containers/span.h"
#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/task_environment.h"
#import "components/favicon/core/large_icon_service_impl.h"
#import "components/favicon/core/test/mock_favicon_service.h"
#import "ios/chrome/app/spotlight/fake_searchable_item_factory.h"
#import "ios/chrome/app/spotlight/fake_spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/skia/include/core/SkBitmap.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using testing::_;
using ui::test::uiimage_utils::UIImageWithSizeAndSolidColor;

namespace {
const char kDummyIconUrl[] = "http://www.example.com/touch_icon.png";
const char kDummyHttpURL1[] = "http://dummyURL1.test/";
const char kDummyHttpURL2[] = "http://dummyURL2.test/";
const char kDummyNonHttpURL[] = "chrome://flags";

favicon_base::FaviconRawBitmapResult CreateTestBitmap(int w, int h) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  CGSize size = CGSizeMake(w, h);
  UIImage* favicon = UIImageWithSizeAndSolidColor(size, [UIColor redColor]);
  NSData* png = UIImagePNGRepresentation(favicon);
  scoped_refptr<base::RefCountedBytes> data(
      new base::RefCountedBytes(base::apple::NSDataToSpan(png)));

  result.bitmap_data = data;
  result.pixel_size = gfx::Size(w, h);
  result.icon_url = GURL(kDummyIconUrl);
  result.icon_type = favicon_base::IconType::kTouchIcon;
  CHECK(result.is_valid());
  return result;
}

}  // namespace

class FakeWebState : public web::FakeWebState {
 public:
  void LoadURL(const GURL& url) {
    SetCurrentURL(url);
    web::FakeNavigationContext context;
    context.SetUrl(url);
    web::FakeNavigationManager* navigation_manager =
        static_cast<web::FakeNavigationManager*>(GetNavigationManager());
    navigation_manager->SetPendingItem(nullptr);
    pending_item_.reset();
    OnNavigationStarted(&context);
    OnNavigationFinished(&context);
  }

 private:
  std::unique_ptr<web::NavigationItem> pending_item_;
};

class OpenTabsSpotlightManagerTest : public PlatformTest {
 public:
  OpenTabsSpotlightManagerTest() {
    CreateMockLargeIconService();
    TestProfileIOS::Builder builder;
    test_profile_ = std::move(builder).Build();
    searchableItemFactory_ = [[FakeSearchableItemFactory alloc]
        initWithDomain:spotlight::DOMAIN_OPEN_TABS];
  }

  void SetUp() override {
    browserList_ = CreateBrowserList();

    fakeSpotlightInterface_ = [[FakeSpotlightInterface alloc] init];

    manager_ = [[OpenTabsSpotlightManager alloc]
        initWithLargeIconService:large_icon_service_.get()
                     browserList:browserList_
              spotlightInterface:fakeSpotlightInterface_
           searchableItemFactory:searchableItemFactory_];

    browser_ = std::make_unique<TestBrowser>(test_profile_.get());
  }

  void TearDown() override { [manager_ shutdown]; }

 protected:
  BrowserList* CreateBrowserList() {
    return BrowserListFactory::GetForProfile(test_profile_.get());
  }

  FakeWebState* CreateWebState(WebStateList* web_state_list) {
    auto test_web_state = std::make_unique<FakeWebState>();
    test_web_state->SetBrowserState(test_profile_.get());
    test_web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    FakeWebState* test_web_state_ptr = test_web_state.get();
    web_state_list->InsertWebState(
        std::move(test_web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return test_web_state_ptr;
  }

  void CreateMockLargeIconService() {
    large_icon_service_.reset(new favicon::LargeIconServiceImpl(
        &mock_favicon_service_, /*image_fetcher=*/nullptr,
        /*desired_size_in_dip_for_server_requests=*/0,
        /*icon_type_for_server_requests=*/favicon_base::IconType::kTouchIcon,
        /*google_server_client_param=*/"test_chrome"));

    EXPECT_CALL(mock_favicon_service_,
                GetLargestRawFaviconForPageURL(_, _, _, _, _))
        .WillRepeatedly([](auto, auto, auto,
                           favicon_base::FaviconRawBitmapCallback callback,
                           base::CancelableTaskTracker* tracker) {
          return tracker->PostTask(
              base::SingleThreadTaskRunner::GetCurrentDefault().get(),
              FROM_HERE,
              base::BindOnce(std::move(callback), CreateTestBitmap(24, 24)));
        });
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> test_profile_;
  FakeSearchableItemFactory* searchableItemFactory_;
  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<favicon::LargeIconServiceImpl> large_icon_service_;
  OpenTabsSpotlightManager* manager_;
  raw_ptr<BrowserList> browserList_;
  FakeSpotlightInterface* fakeSpotlightInterface_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests the clearAndReindexOpenTabs method.
// We are testing that clearAndReindexOpenTabs, actually clears the current
// indexed open tabs by calling spotlight api method
// `deleteSearchableItemsWithDomainIdentifiers` and it reindex the current open
// tabs by calling the spotlight api method `indexSearchableItems`
TEST_F(OpenTabsSpotlightManagerTest, TestClearAndReindexOpenTabs) {
  FakeWebState* tab1 = CreateWebState(browser_.get()->GetWebStateList());

  FakeWebState* tab2 = CreateWebState(browser_.get()->GetWebStateList());

  tab1->LoadURL(GURL(kDummyHttpURL1));
  tab2->LoadURL(GURL(kDummyHttpURL2));

  browserList_->AddBrowser(browser_.get());

  NSUInteger currentIndexedItemCount =
      fakeSpotlightInterface_.indexSearchableItemsCallsCount;

  [manager_ clearAndReindexOpenTabs];

  // Wait for indexing to complete.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    return fakeSpotlightInterface_.indexSearchableItemsCallsCount ==
           currentIndexedItemCount + 2;
  }));

  // Current indexed items should be deleted.
  EXPECT_EQ(fakeSpotlightInterface_
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            1u);

  // Cuurent open tabs (2 tabs) should be reindexed.
  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount,
            currentIndexedItemCount + 2);
}

// Tests that adding a new tab to the browser (with a new url), actually indexes
// a new item in spotlight with that url.
TEST_F(OpenTabsSpotlightManagerTest, TestAddNewTab) {
  browserList_->AddBrowser(browser_.get());

  FakeWebState* tab = CreateWebState(browser_.get()->GetWebStateList());

  tab->LoadURL(GURL(kDummyHttpURL1));

  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount, 1u);
}

// Tests that when we reload a tab with a new url , we actually delete the
// spotlight item that was linked to the previous tab url (if there was no
// remaining tab with that url), and index a new item with the new reloaded url.
TEST_F(OpenTabsSpotlightManagerTest, TestReloadATab) {
  browserList_->AddBrowser(browser_.get());

  FakeWebState* tab = CreateWebState(browser_.get()->GetWebStateList());
  tab->LoadURL(GURL(kDummyHttpURL1));

  // We expect to index the new loaded url.
  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount, 1u);

  // Reload the tab with a different url.
  tab->LoadURL(GURL(kDummyHttpURL2));

  // When we reload we expect to remove the old url since its tab count is
  // only 1.
  EXPECT_EQ(
      fakeSpotlightInterface_.deleteSearchableItemsWithIdentifiersCallsCount,
      1u);

  // We expect that we reindex the new added url (thus the +1 for the calls
  // count.)
  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount, 2u);
}

// Tests that when we add a duplicated tab, we shouldn't add a new spotlight
// item as it is already indexed.
TEST_F(OpenTabsSpotlightManagerTest, TestDuplicateTabs) {
  browserList_->AddBrowser(browser_.get());

  FakeWebState* tab1 = CreateWebState(browser_.get()->GetWebStateList());

  tab1->LoadURL(GURL(kDummyHttpURL1));

  // We expect to index the new loaded tab.
  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount, 1u);

  // Create a new webstate (tab) with the same url.
  FakeWebState* tab2 = CreateWebState(browser_.get()->GetWebStateList());

  tab2->LoadURL(GURL(kDummyHttpURL1));

  // We expect that we won't reindex as it is duplicated (thus the count remains
  // 1).
  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount, 1u);
}

// Tests that invalid http(s) webpages should not be indexed.
TEST_F(OpenTabsSpotlightManagerTest, TestNonHttpWebPageTab) {
  browserList_->AddBrowser(browser_.get());

  FakeWebState* tab1 = CreateWebState(browser_.get()->GetWebStateList());

  // Load the tab with a non http(s) url.
  tab1->LoadURL(GURL(kDummyNonHttpURL));

  // We expect that we won't index this tab as it is non http(s) url so we
  // ignore it.
  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount, 0u);

  // Reload the tab with a valid http(s) url.
  tab1->LoadURL(GURL(kDummyHttpURL1));

  // Once the tab url has changed to a valid url, we expect that we index it.
  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount, 1u);
}

// Tests that closing the only tab that has some link, should lead to remove the
// spotlight item that is linked to that tab url.
TEST_F(OpenTabsSpotlightManagerTest, TestCloseTab) {
  browserList_->AddBrowser(browser_.get());

  FakeWebState* tab1 = CreateWebState(browser_.get()->GetWebStateList());

  tab1->LoadURL(GURL(kDummyHttpURL1));

  // We expect that we will index the added tab.
  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount, 1u);

  // Create a webstate with an existing tab url and add it to the browser
  // webstatlist.
  FakeWebState* tab2 = CreateWebState(browser_.get()->GetWebStateList());

  tab2->LoadURL(GURL(kDummyHttpURL1));

  // Close the first tab.
  browser_.get()->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::CLOSE_USER_ACTION);

  // We don't expect to delete the tab url for spotlight index since there still
  // a tab loaded with that url.
  EXPECT_EQ(
      fakeSpotlightInterface_.deleteSearchableItemsWithIdentifiersCallsCount,
      0u);

  // Close the second tab.
  browser_.get()->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::CLOSE_USER_ACTION);

  // We expect to delete the closed tab (since it was the unique tab that has
  // the loaded url).
  EXPECT_EQ(
      fakeSpotlightInterface_.deleteSearchableItemsWithIdentifiersCallsCount,
      1u);
}

// Tests that when the app is in background, any model updates don't cause an
// immediate effect.
TEST_F(OpenTabsSpotlightManagerTest, TestBackgroundUpdatesPostponed) {
  browserList_->AddBrowser(browser_.get());

  FakeWebState* tab1 = CreateWebState(browser_.get()->GetWebStateList());
  tab1->LoadURL(GURL(kDummyHttpURL1));
  FakeWebState* tab2 = CreateWebState(browser_.get()->GetWebStateList());
  tab2->LoadURL(GURL(kDummyHttpURL2));

  // We expect that we will index the added tabs.
  EXPECT_EQ(fakeSpotlightInterface_.indexSearchableItemsCallsCount, 2u);

  // Enter background
  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationDidEnterBackgroundNotification
                    object:nil
                  userInfo:nil];

  // Close a tab.
  browser_.get()->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::CLOSE_USER_ACTION);

  // We expect to NOT delete the closed tab (since it was the unique tab that
  // has the loaded url).
  EXPECT_EQ(
      fakeSpotlightInterface_.deleteSearchableItemsWithIdentifiersCallsCount,
      0u);

  [[NSNotificationCenter defaultCenter]
      postNotificationName:UIApplicationWillEnterForegroundNotification
                    object:nil
                  userInfo:nil];

  // Since we're expecting the manager to treat any model updates in background
  // as impossible to process immediately, the individual item should not be
  // deleted by ID.
  EXPECT_EQ(
      fakeSpotlightInterface_.deleteSearchableItemsWithIdentifiersCallsCount,
      0u);
  // The manager instead removes everything in its domain.
  EXPECT_EQ(fakeSpotlightInterface_
                .deleteSearchableItemsWithDomainIdentifiersCallsCount,
            1u);
  // Now the manager schedules a reindexing of the only remaining open tab.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    return fakeSpotlightInterface_.indexSearchableItemsCallsCount == 3;
  }));
}
