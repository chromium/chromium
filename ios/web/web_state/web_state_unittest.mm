// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_based_navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/crw_navigation_item_storage.h"
#import "ios/web/public/crw_session_storage.h"
#include "ios/web/public/features.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state_delegate.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_client.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace web {
namespace {

// A text string from the test HTML page in the session storage returned  by
// GetTestSessionStorage().
const char kTestSessionStoragePageText[] = "pony";

// Returns a session storage with a single committed entry of a test HTML page.
CRWSessionStorage* GetTestSessionStorage() {
  base::FilePath path;
  base::PathService::Get(base::DIR_MODULE, &path);
  path = path.Append(
      FILE_PATH_LITERAL("ios/testing/data/http_server_files/pony.html"));
  GURL testFileUrl(base::StringPrintf("file://%s", path.value().c_str()));

  CRWSessionStorage* result = [[CRWSessionStorage alloc] init];
  result.lastCommittedItemIndex = 0;
  CRWNavigationItemStorage* item = [[CRWNavigationItemStorage alloc] init];
  [item setVirtualURL:testFileUrl];
  [result setItemStorages:@[ item ]];
  return result;
}
}  // namespace

using wk_navigation_util::IsWKInternalUrl;

// WebStateTest is parameterized on this enum to test both the legacy
// implementation of navigation manager and the experimental implementation.
enum NavigationManagerChoice {
  TEST_LEGACY_NAVIGATION_MANAGER,
  TEST_WK_BASED_NAVIGATION_MANAGER,
};

// Test fixture for web::WebTest class.
class WebStateTest
    : public WebTestWithWebState,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 protected:
  WebStateTest() {
    if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
      feature_list_.InitAndDisableFeature(
          web::features::kSlimNavigationManager);
    } else {
      feature_list_.InitAndEnableFeature(web::features::kSlimNavigationManager);
    }
  }

  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests script execution with and without callback.
TEST_P(WebStateTest, ScriptExecution) {
  ASSERT_TRUE(LoadHtml("<html></html>"));

  // Execute script without callback.
  web_state()->ExecuteJavaScript(base::UTF8ToUTF16("window.foo = 'bar'"));

  // Execute script with callback.
  __block std::unique_ptr<base::Value> execution_result;
  __block bool execution_complete = false;
  web_state()->ExecuteJavaScript(base::UTF8ToUTF16("window.foo"),
                                 base::BindOnce(^(const base::Value* value) {
                                   execution_result = value->CreateDeepCopy();
                                   execution_complete = true;
                                 }));
  WaitForCondition(^{
    return execution_complete;
  });

  ASSERT_TRUE(execution_result);
  std::string string_result;
  execution_result->GetAsString(&string_result);
  EXPECT_EQ("bar", string_result);
}

// Tests that executing user JavaScript registers user interaction.
TEST_P(WebStateTest, UserScriptExecution) {
  web::TestWebStateDelegate delegate;
  web_state()->SetDelegate(&delegate);
  ASSERT_TRUE(delegate.child_windows().empty());

  ASSERT_TRUE(LoadHtml("<html></html>"));
  web_state()->ExecuteUserJavaScript(@"window.open('', target='_blank');");

  web::TestWebStateDelegate* delegate_ptr = &delegate;
  bool suceess = WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    // Child window can only be open if the user interaction was registered.
    return delegate_ptr->child_windows().size() == 1;
  });

  ASSERT_TRUE(suceess);
  EXPECT_TRUE(delegate.child_windows()[0]);
}

// Tests loading progress.
TEST_P(WebStateTest, LoadingProgress) {
  EXPECT_FLOAT_EQ(0.0, web_state()->GetLoadingProgress());
  ASSERT_TRUE(LoadHtml("<html></html>"));
  WaitForCondition(^bool() {
    return web_state()->GetLoadingProgress() == 1.0;
  });
}

// Tests that page which overrides window.webkit object does not break the
// messaging system.
TEST_P(WebStateTest, OverridingWebKitObject) {
  // Add a script command handler.
  __block bool message_received = false;
  const web::WebState::ScriptCommandCallback callback =
      base::BindRepeating(^bool(const base::DictionaryValue&, const GURL&,
                                /*interacted*/ bool, /*is_main_frame*/ bool,
                                /*sender_frame*/ web::WebFrame*) {
        message_received = true;
        return true;
      });
  web_state()->AddScriptCommandCallback(callback, "test");

  // Load the page which overrides window.webkit object and wait until the
  // test message is received.
  ASSERT_TRUE(LoadHtml(
      "<script>"
      "  webkit = undefined;"
      "  __gCrWeb.message.invokeOnHost({'command': 'test.webkit-overriding'});"
      "</script>"));

  WaitForCondition(^{
    return message_received;
  });
  web_state()->RemoveScriptCommandCallback("test");
}

// Tests that reload with web::ReloadType::NORMAL is no-op when navigation
// manager is empty.
TEST_P(WebStateTest, ReloadWithNormalTypeWithEmptyNavigationManager) {
  NavigationManager* navigation_manager = web_state()->GetNavigationManager();
  ASSERT_FALSE(navigation_manager->GetTransientItem());
  ASSERT_FALSE(navigation_manager->GetPendingItem());
  ASSERT_FALSE(navigation_manager->GetLastCommittedItem());

  navigation_manager->Reload(web::ReloadType::NORMAL,
                             false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager->GetTransientItem());
  ASSERT_FALSE(navigation_manager->GetPendingItem());
  ASSERT_FALSE(navigation_manager->GetLastCommittedItem());
}

// Tests that reload with web::ReloadType::ORIGINAL_REQUEST_URL is no-op when
// navigation manager is empty.
TEST_P(WebStateTest, ReloadWithOriginalTypeWithEmptyNavigationManager) {
  NavigationManager* navigation_manager = web_state()->GetNavigationManager();
  ASSERT_FALSE(navigation_manager->GetTransientItem());
  ASSERT_FALSE(navigation_manager->GetPendingItem());
  ASSERT_FALSE(navigation_manager->GetLastCommittedItem());

  navigation_manager->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                             false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager->GetTransientItem());
  ASSERT_FALSE(navigation_manager->GetPendingItem());
  ASSERT_FALSE(navigation_manager->GetLastCommittedItem());
}

// Tests that the snapshot method returns an image of a rendered html page.
TEST_P(WebStateTest, Snapshot) {
  ASSERT_TRUE(
      LoadHtml("<html><div style='background-color:#FF0000; width:50%; "
               "height:100%;'></div></html>"));
  __block bool snapshot_complete = false;
  [[[UIApplication sharedApplication] keyWindow]
      addSubview:web_state()->GetView()];
  // The subview is added but not immediately painted, so a small delay is
  // necessary.
  CGRect rect = [web_state()->GetView() bounds];
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.2));
  web_state()->TakeSnapshot(
      rect, base::BindOnce(^(gfx::Image snapshot) {
        if (@available(iOS 11, *)) {
          ASSERT_FALSE(snapshot.IsEmpty());
          EXPECT_GT(snapshot.Width(), 0);
          EXPECT_GT(snapshot.Height(), 0);
          int red_pixel_x = (snapshot.Width() / 2) - 10;
          int white_pixel_x = (snapshot.Width() / 2) + 10;
          // Test a pixel on the left (red) side.
          gfx::test::CheckColors(
              gfx::test::GetPlatformImageColor(
                  gfx::test::ToPlatformType(snapshot), red_pixel_x, 50),
              SK_ColorRED);
          // Test a pixel on the right (white) side.
          gfx::test::CheckColors(
              gfx::test::GetPlatformImageColor(
                  gfx::test::ToPlatformType(snapshot), white_pixel_x, 50),
              SK_ColorWHITE);
        }
        snapshot_complete = true;
      }));
  WaitForCondition(^{
    return snapshot_complete;
  });
}

// Tests that message sent from main frame triggers the ScriptCommandCallback
// with |is_main_frame| = true.
TEST_P(WebStateTest, MessageFromMainFrame) {
  // Add a script command handler.
  __block bool message_received = false;
  __block bool message_from_main_frame = false;
  __block base::Value message_value;
  const web::WebState::ScriptCommandCallback callback = base::BindRepeating(
      ^bool(const base::DictionaryValue& value, const GURL&,
            bool user_interacted, bool is_main_frame, WebFrame* sender_frame) {
        message_received = true;
        message_from_main_frame = is_main_frame;
        message_value = value.Clone();
        return true;
      });
  web_state()->AddScriptCommandCallback(callback, "test");

  ASSERT_TRUE(LoadHtml(
      "<script>"
      "  __gCrWeb.message.invokeOnHost({'command': 'test.from-main-frame'});"
      "</script>"));

  WaitForCondition(^{
    return message_received;
  });
  web_state()->RemoveScriptCommandCallback("test");
  EXPECT_TRUE(message_from_main_frame);
  EXPECT_TRUE(message_value.is_dict());
  EXPECT_EQ(message_value.DictSize(), size_t(1));
  base::Value* command = message_value.FindKey("command");
  EXPECT_NE(command, nullptr);
  EXPECT_TRUE(command->is_string());
  EXPECT_EQ(command->GetString(), "test.from-main-frame");
}

// Tests that message sent from main frame triggers the ScriptCommandCallback
// with |is_main_frame| = false.
TEST_P(WebStateTest, MessageFromIFrame) {
  // Add a script command handler.
  __block bool message_received = false;
  __block bool message_from_main_frame = false;
  __block base::Value message_value;
  const web::WebState::ScriptCommandCallback callback = base::BindRepeating(
      ^bool(const base::DictionaryValue& value, const GURL&,
            bool user_interacted, bool is_main_frame, WebFrame* sender_frame) {
        message_received = true;
        message_from_main_frame = is_main_frame;
        message_value = value.Clone();
        return true;
      });
  web_state()->AddScriptCommandCallback(callback, "test");

  ASSERT_TRUE(LoadHtml(
      "<iframe srcdoc='"
      "<script>"
      "  __gCrWeb.message.invokeOnHost({\"command\": \"test.from-iframe\"});"
      "</script>"
      "'/>"));

  WaitForCondition(^{
    return message_received;
  });
  web_state()->RemoveScriptCommandCallback("test");
  EXPECT_FALSE(message_from_main_frame);
  EXPECT_TRUE(message_value.is_dict());
  EXPECT_EQ(message_value.DictSize(), size_t(1));
  base::Value* command = message_value.FindKey("command");
  EXPECT_NE(command, nullptr);
  EXPECT_TRUE(command->is_string());
  EXPECT_EQ(command->GetString(), "test.from-iframe");
}

// Tests that the web state has an opener after calling SetHasOpener().
TEST_P(WebStateTest, SetHasOpener) {
  ASSERT_FALSE(web_state()->HasOpener());
  web_state()->SetHasOpener(true);
  EXPECT_TRUE(web_state()->HasOpener());
}

// Verifies that large session can be restored. SlimNavigationManagder has max
// session size limit of |wk_navigation_util::kMaxSessionSize|.
TEST_P(WebStateTest, RestoreLargeSession) {
  // Create session storage with large number of items.
  const int kItemCount = 150;
  NSMutableArray<CRWNavigationItemStorage*>* item_storages =
      [NSMutableArray arrayWithCapacity:kItemCount];
  for (unsigned int i = 0; i < kItemCount; i++) {
    CRWNavigationItemStorage* item = [[CRWNavigationItemStorage alloc] init];
    item.virtualURL = GURL(base::StringPrintf("http://www.%u.com", i));
    item.title = base::ASCIIToUTF16(base::StringPrintf("Test%u", i));
    [item_storages addObject:item];
  }

  // Restore the session.
  WebState::CreateParams params(GetBrowserState());
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.itemStorages = item_storages;
  auto web_state = WebState::CreateWithStorageSession(params, session_storage);
  WebState* web_state_ptr = web_state.get();
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/873729): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  // Verify that session was fully restored.
  int kExpectedItemCount = web::GetWebClient()->IsSlimNavigationManagerEnabled()
                               ? wk_navigation_util::kMaxSessionSize
                               : kItemCount;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    bool restored = navigation_manager->GetItemCount() == kExpectedItemCount &&
                    navigation_manager->CanGoForward();
    if (!restored) {
      EXPECT_FALSE(navigation_manager->CanGoForward());
      // TODO(crbug.com/877671): Ensure that the following API work correctly:
      //  - WebState::GetLastCommittedURL
      //  - NavigationManager::GetBackwardItems
      //  - NavigationManager::GetForwardItems
      //  - NavigationManager::GetLastCommittedItem
      //  - NavigationManager::GetPendingItem
      //  - NavigationManager::GetLastCommittedItemIndex
      //  - NavigationManager::GetPendingItemIndex
    } else {
      EXPECT_EQ("http://www.0.com/", web_state_ptr->GetLastCommittedURL());
      NavigationItem* last_committed_item =
          navigation_manager->GetLastCommittedItem();
      EXPECT_TRUE(last_committed_item);
      EXPECT_TRUE(last_committed_item &&
                  last_committed_item->GetURL() == "http://www.0.com/");
      EXPECT_EQ(0, navigation_manager->GetLastCommittedItemIndex());
      EXPECT_TRUE(navigation_manager->GetBackwardItems().empty());
      EXPECT_EQ(std::max(navigation_manager->GetItemCount() - 1, 0),
                static_cast<int>(navigation_manager->GetForwardItems().size()));
    }
    // TODO(crbug.com/877671): Ensure that the following API work correctly:
    //  - WebState::GetTitle
    //  - WebState::GetLoadingProgress
    EXPECT_FALSE(web_state_ptr->IsCrashed());
    EXPECT_FALSE(web_state_ptr->IsEvicted());
    EXPECT_EQ("http://www.0.com/", web_state_ptr->GetVisibleURL());
    NavigationItem* visible_item = navigation_manager->GetVisibleItem();
    EXPECT_TRUE(visible_item);
    EXPECT_TRUE(visible_item && visible_item->GetURL() == "http://www.0.com/");
    EXPECT_FALSE(navigation_manager->CanGoBack());
    EXPECT_FALSE(navigation_manager->GetTransientItem());
    EXPECT_FALSE(IsWKInternalUrl(web_state_ptr->GetVisibleURL()));

    return restored;
  }));
  EXPECT_EQ(kExpectedItemCount, navigation_manager->GetItemCount());
  EXPECT_TRUE(navigation_manager->CanGoForward());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 100, 1);
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    histogram_tester_.ExpectTotalCount(kRestoreNavigationTime, 1);
  }

  // Now wait until the last committed item is fully loaded.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    EXPECT_FALSE(IsWKInternalUrl(web_state_ptr->GetVisibleURL()));

    return !navigation_manager->GetPendingItem() && !web_state_ptr->IsLoading();
  }));
}

// Tests that if a saved session is provided when creating a new WebState, it is
// restored after the first NavigationManager::LoadIfNecessary() call.
TEST_P(WebStateTest, RestoredFromHistory) {
  auto web_state = WebState::CreateWithStorageSession(
      WebState::CreateParams(GetBrowserState()), GetTestSessionStorage());

  ASSERT_FALSE(test::IsWebViewContainingText(web_state.get(),
                                             kTestSessionStoragePageText));
  web_state->GetNavigationManager()->LoadIfNecessary();
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state.get(),
                                                 kTestSessionStoragePageText));
}

// Tests that NavigationManager::LoadIfNecessary() restores the page after
// disabling and re-enabling web usage.
TEST_P(WebStateTest, DisableAndReenableWebUsage) {
  auto web_state = WebState::CreateWithStorageSession(
      WebState::CreateParams(GetBrowserState()), GetTestSessionStorage());
  web_state->GetNavigationManager()->LoadIfNecessary();
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state.get(),
                                                 kTestSessionStoragePageText));

  web_state->SetWebUsageEnabled(false);
  web_state->SetWebUsageEnabled(true);

  // NavigationManager::LoadIfNecessary() should restore the page.
  ASSERT_FALSE(test::IsWebViewContainingText(web_state.get(),
                                             kTestSessionStoragePageText));
  web_state->GetNavigationManager()->LoadIfNecessary();
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state.get(),
                                                 kTestSessionStoragePageText));
}

INSTANTIATE_TEST_CASE_P(
    ProgrammaticWebStateTest,
    WebStateTest,
    ::testing::Values(
        NavigationManagerChoice::TEST_LEGACY_NAVIGATION_MANAGER,
        NavigationManagerChoice::TEST_WK_BASED_NAVIGATION_MANAGER));

}  // namespace web
