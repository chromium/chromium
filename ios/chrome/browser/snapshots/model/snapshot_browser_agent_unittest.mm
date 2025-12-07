// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"

#import <UIKit/UIKit.h>

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/snapshots/model/fake_snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Name of the directory where snapshots are saved.
const char kIdentifier[] = "Identifier";

// Converts `snapshot_id` to a SnapshotIDWrapper.
SnapshotIDWrapper* ToWrapper(SnapshotID snapshot_id) {
  return [[SnapshotIDWrapper alloc] initWithSnapshotID:snapshot_id];
}

// Returns a callback that capture its argument and store it to `output`.
template <typename T>
base::OnceCallback<void(T)> CaptureArg(T& output) {
  return base::BindOnce([](T& output, T arg) { output = arg; },
                        std::ref(output));
}

}  // anonymous namespace

class SnapshotBrowserAgentTest : public PlatformTest {
 public:
  SnapshotBrowserAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Configure the FakeSnapshotGeneratorDelegate with a fake view. This
    // allow to capture snapshot with a fake WebState.
    delegate_ = [[FakeSnapshotGeneratorDelegate alloc] init];

    UIView* view = [[UIView alloc] initWithFrame:{{0, 0}, {300, 400}}];
    view.backgroundColor = [UIColor redColor];
    delegate_.view = view;

    UIWindow* window = GetAnyKeyWindow();
    [window addSubview:delegate_.view];
    [window makeKeyAndVisible];

    // Hack to forcefully render the view to successfully capture a snapshot.
    [NSRunLoop.currentRunLoop
        runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    [window layoutIfNeeded];
  }

  void TearDown() override { [delegate_.view removeFromSuperview]; }

  // Create a fake WebState that is configured to take snapshot using the
  // fake SnapshotGeneratorDelegate.
  std::unique_ptr<web::WebState> CreateWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    SnapshotTabHelper::CreateForWebState(web_state.get());
    SnapshotSourceTabHelper::CreateForWebState(web_state.get());
    SnapshotTabHelper::FromWebState(web_state.get())->SetDelegate(delegate_);
    return web_state;
  }

  // Updates snapshot for WebState and returns it.
  UIImage* UpdateSnapshotForWebState(web::WebState* web_state) {
    UIImage* result = nil;
    base::RunLoop run_loop;
    SnapshotTabHelper::FromWebState(web_state)->UpdateSnapshotWithCallback(
        base::CallbackToBlock(
            base::BindOnce(CaptureArg(result)).Then(run_loop.QuitClosure())));

    run_loop.Run();
    return result;
  }

  // Retrieves an image from the storage (or null if it is not present).
  UIImage* RetrieveImageFromStorage(SnapshotID snapshot_id) {
    auto* storage =
        SnapshotBrowserAgent::FromBrowser(browser_.get())->snapshot_storage();

    UIImage* result = nil;
    base::RunLoop run_loop;
    [storage retrieveImageWithSnapshotID:ToWrapper(snapshot_id)
                            snapshotKind:SnapshotKindColor
                              completion:base::CallbackToBlock(
                                             CaptureArg(result).Then(
                                                 run_loop.QuitClosure()))];

    run_loop.Run();
    return result;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  FakeSnapshotGeneratorDelegate* delegate_;
};

TEST_F(SnapshotBrowserAgentTest, SnapshotStorageCreatedAfterSettingSessionID) {
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent* agent =
      SnapshotBrowserAgent::FromBrowser(browser_.get());
  EXPECT_NE(nullptr, agent);
  EXPECT_EQ(nil, agent->snapshot_storage());
  agent->SetSessionID(kIdentifier);
  EXPECT_NE(nil, agent->snapshot_storage());
}

// Tests that snapshots are deleted when a tab is closed by the user.
TEST_F(SnapshotBrowserAgentTest, SnapshotDeletedWhenTabClosedByUser) {
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent::FromBrowser(browser_.get())->SetSessionID(kIdentifier);

  // Create a fake WebState and insert it in the Browser's WebStateList.
  browser_->GetWebStateList()->InsertWebState(CreateWebState());
  ASSERT_EQ(browser_->GetWebStateList()->count(), 1);

  // Generate the snapshot for the web_state.
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  const SnapshotID snapshot_id(web_state->GetUniqueIdentifier());
  UIImage* image = UpdateSnapshotForWebState(web_state);
  ASSERT_NSNE(image, nil);

  // The snapshot should now be retrievable from the storage.
  EXPECT_NSEQ(RetrieveImageFromStorage(snapshot_id), image);

  // Pretend the user closed the WebState and check that the image has been
  // removed from the storage.
  const auto close_reason = WebStateList::ClosingReason::kUserAction;
  browser_->GetWebStateList()->CloseWebStateAt(0, close_reason);
  EXPECT_NSEQ(RetrieveImageFromStorage(snapshot_id), nil);
}

// Tests that snapshots are deleted when a tab is closed for cleanup.
TEST_F(SnapshotBrowserAgentTest, SnapshotDeletedWhenTabClosedForCleanup) {
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent::FromBrowser(browser_.get())->SetSessionID(kIdentifier);

  // Create a fake WebState and insert it in the Browser's WebStateList.
  browser_->GetWebStateList()->InsertWebState(CreateWebState());
  ASSERT_EQ(browser_->GetWebStateList()->count(), 1);

  // Generate the snapshot for the web_state.
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  const SnapshotID snapshot_id(web_state->GetUniqueIdentifier());
  UIImage* image = UpdateSnapshotForWebState(web_state);
  ASSERT_NSNE(image, nil);

  // The snapshot should now be retrievable from the storage.
  EXPECT_NSEQ(RetrieveImageFromStorage(snapshot_id), image);

  // Pretend the user closed the WebState and check that the image has been
  // removed from the storage.
  const auto close_reason = WebStateList::ClosingReason::kTabsCleanup;
  browser_->GetWebStateList()->CloseWebStateAt(0, close_reason);
  EXPECT_NSEQ(RetrieveImageFromStorage(snapshot_id), nil);
}

// Tests that snapshots are not deleted when a tab is closed automatically.
TEST_F(SnapshotBrowserAgentTest, SnapshotNotDeletedWhenTabClosedWithNoFlags) {
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent::FromBrowser(browser_.get())->SetSessionID(kIdentifier);

  // Create a fake WebState and insert it in the Browser's WebStateList.
  browser_->GetWebStateList()->InsertWebState(CreateWebState());
  ASSERT_EQ(browser_->GetWebStateList()->count(), 1);

  // Generate the snapshot for the web_state.
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  const SnapshotID snapshot_id(web_state->GetUniqueIdentifier());
  UIImage* image = UpdateSnapshotForWebState(web_state);
  ASSERT_NSNE(image, nil);

  // The snapshot should now be retrievable from the storage.
  EXPECT_NSEQ(RetrieveImageFromStorage(snapshot_id), image);

  // Pretend the user closed the WebState and check that the image has been
  // removed from the storage.
  const auto close_reason = WebStateList::ClosingReason::kDefault;
  browser_->GetWebStateList()->CloseWebStateAt(0, close_reason);
  EXPECT_NSEQ(RetrieveImageFromStorage(snapshot_id), image);
}

// Tests that snapshots are not deleted when a tab is detached.
TEST_F(SnapshotBrowserAgentTest, SnapshotNotDeletedWhenTabDetached) {
  SnapshotBrowserAgent::CreateForBrowser(browser_.get());
  SnapshotBrowserAgent::FromBrowser(browser_.get())->SetSessionID(kIdentifier);

  // Create a fake WebState and insert it in the Browser's WebStateList.
  browser_->GetWebStateList()->InsertWebState(CreateWebState());
  ASSERT_EQ(browser_->GetWebStateList()->count(), 1);

  // Generate the snapshot for the web_state.
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(0);
  const SnapshotID snapshot_id(web_state->GetUniqueIdentifier());
  UIImage* image = UpdateSnapshotForWebState(web_state);
  ASSERT_NSNE(image, nil);

  // The snapshot should now be retrievable from the storage.
  EXPECT_NSEQ(RetrieveImageFromStorage(snapshot_id), image);

  // Pretend the user closed the WebState and check that the image has been
  // removed from the storage.
  auto detached_web_state = browser_->GetWebStateList()->DetachWebStateAt(0);
  EXPECT_NSEQ(RetrieveImageFromStorage(snapshot_id), image);
}
