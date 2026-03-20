// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/inactive_tabs/inactive_tabs_mediator.h"

#import "base/barrier_closure.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/fake_tab_collection_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/web_state_tab_switcher_item.h"
#import "ios/chrome/browser/tabs/model/tabs_closer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

// Builds a `TestFaviconLoader`.
std::unique_ptr<KeyedService> BuildTestFaviconLoader(ProfileIOS* profile) {
  return std::make_unique<TestFaviconLoader>();
}

// The identifier where the snapshots are saved.
const char kIdentifier[] = "Identifier";

}  // namespace

@interface InactiveTabsMediator (Test) <SnapshotStorageObserver>
- (void)didUpdateSnapshotStorageWithSnapshotID:(SnapshotIDWrapper*)snapshotID;
@end

// Test fixture for InactiveTabsMediator.
class InactiveTabsMediatorTest : public PlatformTest {
 public:
  InactiveTabsMediatorTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(IOSChromeFaviconLoaderFactory::GetInstance(),
                              base::BindOnce(&BuildTestFaviconLoader));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    SnapshotBrowserAgent::CreateForBrowser(browser_.get());
    SnapshotBrowserAgent::FromBrowser(browser_.get())
        ->SetSessionID(kIdentifier);

    FaviconLoader* favicon_loader =
        IOSChromeFaviconLoaderFactory::GetForProfile(profile_.get());

    mediator_ = [[InactiveTabsMediator alloc]
        initWithWebStateList:browser_->GetWebStateList()
          profilePrefService:profile_->GetPrefs()
               faviconLoader:favicon_loader
        snapshotBrowserAgent:SnapshotBrowserAgent::FromBrowser(browser_.get())
                  tabsCloser:std::make_unique<TabsCloser>(
                                 browser_.get(),
                                 TabsCloser::ClosePolicy::kAllTabs)];
    consumer_ = [[FakeTabCollectionConsumer alloc] init];
    mediator_.consumer = consumer_;
  }

  ~InactiveTabsMediatorTest() override { [mediator_ disconnect]; }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  InactiveTabsMediator* mediator_;
  FakeTabCollectionConsumer* consumer_;
};

// Tests that `fetchTabSnapshotAndFavicon:completion:` is calling `completion`
// twice.
TEST_F(InactiveTabsMediatorTest, FetchTabSnapshotAndFavicon) {
  auto fake_web_state = std::make_unique<web::FakeWebState>();
  SnapshotTabHelper::CreateForWebState(fake_web_state.get());
  SnapshotSourceTabHelper::CreateForWebState(fake_web_state.get());

  WebStateTabSwitcherItem* item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:fake_web_state.get()];

  // Expects the completion to be called twice.
  base::RunLoop run_loop;
  auto barrier = base::CallbackToBlock(
      base::IgnoreArgs<TabSwitcherItem*, TabSnapshotAndFavicon*>(
          base::BarrierClosure(2, run_loop.QuitClosure())));

  [mediator_ fetchTabSnapshotAndFavicon:item completion:barrier];
  run_loop.Run();
}

// Tests that the snapshot update is ignored during a batch operation and
// the feature is enabled.
TEST_F(InactiveTabsMediatorTest, SnapshotIgnoredBatchOperationGuardTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kGridMediatorSnapshotUpdateBatchGuard);

  // Add a web state to the list using a batch operation to avoid iPad-only
  // insertion check.
  web::WebState* web_state_ptr = nullptr;
  {
    WebStateList::ScopedBatchOperation lock =
        browser_->GetWebStateList()->StartBatchOperation();
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    SnapshotTabHelper::CreateForWebState(web_state.get());
    web_state_ptr = web_state.get();
    browser_->GetWebStateList()->InsertWebState(std::move(web_state));
  }

  SnapshotID snapshot_id = SnapshotID(web_state_ptr->GetUniqueIdentifier());

  SnapshotIDWrapper* wrapper =
      [[SnapshotIDWrapper alloc] initWithSnapshotID:snapshot_id];

  // When no batch operation is in progress, snapshot update should be
  // processed.
  NSUInteger initial_count = consumer_.replaceItemCount;
  [mediator_ didUpdateSnapshotStorageWithSnapshotID:wrapper];
  EXPECT_EQ(initial_count + 1, consumer_.replaceItemCount);

  // When a batch operation is in progress, snapshot update should be ignored.
  {
    WebStateList::ScopedBatchOperation lock =
        browser_->GetWebStateList()->StartBatchOperation();
    [mediator_ didUpdateSnapshotStorageWithSnapshotID:wrapper];
    EXPECT_EQ(initial_count + 1, consumer_.replaceItemCount);
  }

  // When the batch operation ends, snapshot update should be processed.
  [mediator_ didUpdateSnapshotStorageWithSnapshotID:wrapper];
  EXPECT_EQ(initial_count + 2, consumer_.replaceItemCount);
}
