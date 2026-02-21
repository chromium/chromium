// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/inactive_tabs/inactive_tabs_mediator.h"

#import "base/barrier_closure.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
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
  }

  ~InactiveTabsMediatorTest() override { [mediator_ disconnect]; }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  InactiveTabsMediator* mediator_;
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
