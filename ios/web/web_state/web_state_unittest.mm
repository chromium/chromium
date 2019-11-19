// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_based_navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/fakes/test_web_state_delegate.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_client.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/test/test_url_constants.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/gfx/geometry/rect_f.h"
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

// A text string that is included in |kTestPageHTML|.
const char kTextInTestPageHTML[] = "this_is_a_test_string";

// A test page HTML containing |kTextInTestPageHTML|.
const char kTestPageHTML[] = "<html><body>this_is_a_test_string</body><html>";
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
    : public TestWebClient,
      public WebTestWithWebState,
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
  const web::WebState::ScriptCommandCallback callback = base::BindRepeating(
      ^(const base::DictionaryValue&, const GURL&,
        /*interacted*/ bool, /*is_main_frame*/ web::WebFrame*) {
        message_received = true;
      });
  auto subscription = web_state()->AddScriptCommandCallback(callback, "test");

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
      gfx::RectF(rect), base::BindRepeating(^(const gfx::Image& snapshot) {
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
  const web::WebState::ScriptCommandCallback callback =
      base::BindRepeating(^(const base::DictionaryValue& value, const GURL&,
                            bool user_interacted, WebFrame* sender_frame) {
        message_received = true;
        message_from_main_frame = sender_frame->IsMainFrame();
        message_value = value.Clone();
      });
  auto subscription = web_state()->AddScriptCommandCallback(callback, "test");

  ASSERT_TRUE(LoadHtml(
      "<script>"
      "  __gCrWeb.message.invokeOnHost({'command': 'test.from-main-frame'});"
      "</script>"));

  WaitForCondition(^{
    return message_received;
  });
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
  const web::WebState::ScriptCommandCallback callback =
      base::BindRepeating(^(const base::DictionaryValue& value, const GURL&,
                            bool user_interacted, WebFrame* sender_frame) {
        message_received = true;
        message_from_main_frame = sender_frame->IsMainFrame();
        message_value = value.Clone();
      });
  auto subscription = web_state()->AddScriptCommandCallback(callback, "test");

  ASSERT_TRUE(LoadHtml(
      "<iframe srcdoc='"
      "<script>"
      "  __gCrWeb.message.invokeOnHost({\"command\": \"test.from-iframe\"});"
      "</script>"
      "'/>"));

  WaitForCondition(^{
    return message_received;
  });
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
  web_state->SetKeepRenderProcessAlive(true);
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
    EXPECT_EQ(restored, !navigation_manager->IsRestoreSessionInProgress());
    if (!restored) {
      EXPECT_FALSE(navigation_manager->GetLastCommittedItem());
      EXPECT_EQ(-1, navigation_manager->GetLastCommittedItemIndex());
      EXPECT_TRUE(web_state_ptr->GetLastCommittedURL().is_empty());
      EXPECT_FALSE(navigation_manager->CanGoForward());
      EXPECT_TRUE(navigation_manager->GetBackwardItems().empty());
      EXPECT_TRUE(navigation_manager->GetForwardItems().empty());
      EXPECT_EQ("Test0", base::UTF16ToASCII(web_state_ptr->GetTitle()));
      EXPECT_EQ(0.0, web_state_ptr->GetLoadingProgress());
      EXPECT_EQ(-1, navigation_manager->GetPendingItemIndex());
      EXPECT_FALSE(navigation_manager->GetPendingItem());
    } else {
      EXPECT_EQ("Test0", base::UTF16ToASCII(web_state_ptr->GetTitle()));
      NavigationItem* last_committed_item =
          navigation_manager->GetLastCommittedItem();
      // After restoration is complete GetLastCommittedItem() will return null
      // until fist post-restore navigation is finished.
      if (last_committed_item) {
        EXPECT_EQ("http://www.0.com/", last_committed_item->GetURL());
        EXPECT_EQ("http://www.0.com/", web_state_ptr->GetLastCommittedURL());
        EXPECT_EQ(0, navigation_manager->GetLastCommittedItemIndex());
        EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
            navigation_manager->GetLastCommittedItem()->GetTransitionType(),
            ui::PAGE_TRANSITION_RELOAD));
      } else {
        EXPECT_EQ("", web_state_ptr->GetLastCommittedURL());
        EXPECT_EQ(-1, navigation_manager->GetLastCommittedItemIndex());
        NavigationItem* pending_item = navigation_manager->GetPendingItem();
        EXPECT_TRUE(pending_item);
        if (pending_item) {
          EXPECT_EQ("http://www.0.com/", pending_item->GetURL());
        }
      }
      EXPECT_TRUE(navigation_manager->GetBackwardItems().empty());
      EXPECT_EQ(std::max(navigation_manager->GetItemCount() - 1, 0),
                static_cast<int>(navigation_manager->GetForwardItems().size()));
    }
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

    return !navigation_manager->GetPendingItem() &&
           !web_state_ptr->IsLoading() &&
           web_state_ptr->GetLoadingProgress() == 1.0;
  }));

  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
}

// Verifies that calling WebState::Stop() does not stop the session restoration.
// Session restoration should be opaque to the user and embedder, so calling
// Stop() is no-op.
TEST_P(WebStateTest, CallStopDuringSessionRestore) {
  // Create session storage with large number of items.
  const int kItemCount = 10;
  NSMutableArray<CRWNavigationItemStorage*>* item_storages =
      [NSMutableArray arrayWithCapacity:kItemCount];
  for (unsigned int i = 0; i < kItemCount; i++) {
    CRWNavigationItemStorage* item = [[CRWNavigationItemStorage alloc] init];
    item.virtualURL = GURL(base::StringPrintf("http://www.%u.com", i));
    [item_storages addObject:item];
  }

  // Restore the session.
  WebState::CreateParams params(GetBrowserState());
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.itemStorages = item_storages;
  auto web_state = WebState::CreateWithStorageSession(params, session_storage);
  web_state->SetKeepRenderProcessAlive(true);
  WebState* web_state_ptr = web_state.get();
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/873729): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  // Verify that session was fully restored.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    bool restored = navigation_manager->GetItemCount() == kItemCount &&
                    navigation_manager->CanGoForward();
    if (!restored) {
      web_state_ptr->Stop();  // Attempt to interrupt the session restoration.
    }
    return restored;
  }));
  EXPECT_EQ(kItemCount, navigation_manager->GetItemCount());
  EXPECT_TRUE(navigation_manager->CanGoForward());

  // Now wait until the last committed item is fully loaded.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !navigation_manager->GetPendingItem() && !web_state_ptr->IsLoading();
  }));
}

// Verifies that calling NavigationManager::LoadURLWithParams() does not stop
// the session restoration and eventually loads the requested URL.
TEST_P(WebStateTest, CallLoadURLWithParamsDuringSessionRestore) {
  // Create session storage with large number of items.
  const int kItemCount = 10;
  NSMutableArray<CRWNavigationItemStorage*>* item_storages =
      [NSMutableArray arrayWithCapacity:kItemCount];
  for (unsigned int i = 0; i < kItemCount; i++) {
    CRWNavigationItemStorage* item = [[CRWNavigationItemStorage alloc] init];
    item.virtualURL = GURL(base::StringPrintf("http://www.%u.test", i));
    item.userAgentType = UserAgentType::MOBILE;
    [item_storages addObject:item];
  }

  // Restore the session.
  WebState::CreateParams params(GetBrowserState());
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.itemStorages = item_storages;
  auto web_state = WebState::CreateWithStorageSession(params, session_storage);
  web_state->SetKeepRenderProcessAlive(true);
  WebState* web_state_ptr = web_state.get();
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/873729): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  // Attempt to interrupt the session restoration.
  GURL url("http://foo.test/");
  NavigationManager::WebLoadParams load_params(url);
  navigation_manager->LoadURLWithParams(load_params);

  // Verify that session was fully restored.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    bool restored = navigation_manager->GetItemCount() == kItemCount &&
                    navigation_manager->CanGoForward();
    if (!restored) {
      // Attempt to interrupt the session restoration multiple times, which is
      // something that the user can do on the slow network.
      navigation_manager->LoadURLWithParams(load_params);
    }
    return restored;
  }));
  EXPECT_EQ(kItemCount, navigation_manager->GetItemCount());
  EXPECT_TRUE(navigation_manager->CanGoForward());

  // Now wait until the last committed item is fully loaded.
  // TODO(crbug.com/996544) On Xcode 11 beta 6 this became very slow.  This
  // appears to only affect simulator, and will hopefully be fixed in a future
  // Xcode release.  Revert this to |kWaitForPageLoadTimeout| alone when fixed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout * 5, ^{
    return web_state_ptr->GetLastCommittedURL() == url;
  }));
}

// Verifies that calling NavigationManager::Reload() does not stop the session
// restoration. Session restoration should be opaque to the user and embedder,
// so calling Reload() is no-op.
TEST_P(WebStateTest, CallReloadDuringSessionRestore) {
  // Create session storage with large number of items.
  const int kItemCount = 10;
  NSMutableArray<CRWNavigationItemStorage*>* item_storages =
      [NSMutableArray arrayWithCapacity:kItemCount];
  for (unsigned int i = 0; i < kItemCount; i++) {
    CRWNavigationItemStorage* item = [[CRWNavigationItemStorage alloc] init];
    item.virtualURL = GURL(base::StringPrintf("http://www.%u.com", i));
    [item_storages addObject:item];
  }

  // Restore the session.
  WebState::CreateParams params(GetBrowserState());
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.itemStorages = item_storages;
  auto web_state = WebState::CreateWithStorageSession(params, session_storage);
  web_state->SetKeepRenderProcessAlive(true);
  WebState* web_state_ptr = web_state.get();
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/873729): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  // Verify that session was fully restored.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    bool restored = navigation_manager->GetItemCount() == kItemCount &&
                    navigation_manager->CanGoForward();
    if (!restored) {
      // Attempt to interrupt the session restoration.
      navigation_manager->Reload(web::ReloadType::NORMAL,
                                 /*check_for_repost=*/false);
    }
    return restored;
  }));
  EXPECT_EQ(kItemCount, navigation_manager->GetItemCount());
  EXPECT_TRUE(navigation_manager->CanGoForward());

  // Now wait until the last committed item is fully loaded.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !navigation_manager->GetPendingItem() && !web_state_ptr->IsLoading();
  }));
}

// Verifies that each page title is restored.
TEST_P(WebStateTest, RestorePageTitles) {
  // Create session storage.
  const int kItemCount = 3;
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
  web_state->SetKeepRenderProcessAlive(true);
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  // TODO(crbug.com/873729): The session will not be restored until
  // LoadIfNecessary call. Fix the bug and remove extra call.
  navigation_manager->LoadIfNecessary();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return navigation_manager->GetItemCount() == kItemCount;
  }));

  for (unsigned int i = 0; i < kItemCount; i++) {
    NavigationItem* item = navigation_manager->GetItemAtIndex(i);
    EXPECT_EQ(GURL(base::StringPrintf("http://www.%u.com", i)),
              item->GetVirtualURL());
    EXPECT_EQ(base::ASCIIToUTF16(base::StringPrintf("Test%u", i)),
              item->GetTitle());
  }
}

// Tests that loading an HTML page after a failed navigation works.
TEST_P(WebStateTest, LoadChromeThenHTML) {
  GURL app_specific_url(
      base::StringPrintf("%s://app_specific_url", kTestAppSpecificScheme));
  web::NavigationManager::WebLoadParams load_params(app_specific_url);
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
  // Wait for the error loading and check that it corresponds with
  // kUnsupportedUrlErrorPage.
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(),
      testing::GetErrorText(web_state(), app_specific_url, "NSURLErrorDomain",
                            /*error_code=*/NSURLErrorUnsupportedURL,
                            /*is_post=*/false, /*is_otr=*/false,
                            /*has_ssl_info=*/false)));
  NSString* data_html = @(kTestPageHTML);
  web_state()->LoadData([data_html dataUsingEncoding:NSUTF8StringEncoding],
                        @"text/html", GURL("https://www.chromium.org"));
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kTextInTestPageHTML));
}

// Tests that loading an arbitrary file URL is a no-op.
TEST_P(WebStateTest, LoadFileURL) {
  GURL file_url("file:///path/to/file.html");
  web::NavigationManager::WebLoadParams load_params(file_url);
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);
  EXPECT_FALSE(web_state()->IsLoading());
}

// Tests that reloading after loading HTML page will load the online page.
TEST_P(WebStateTest, LoadChromeThenWaitThenHTMLThenReload) {
  net::EmbeddedTestServer server;
  net::test_server::RegisterDefaultHandlers(&server);
  ASSERT_TRUE(server.Start());
  GURL echo_url = server.GetURL("/echo");

  GURL app_specific_url(
      base::StringPrintf("%s://app_specific_url", kTestAppSpecificScheme));
  web::NavigationManager::WebLoadParams load_params(app_specific_url);
  web_state()->GetNavigationManager()->LoadURLWithParams(load_params);
  // Wait for the error loading.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(),
      testing::GetErrorText(web_state(), app_specific_url, "NSURLErrorDomain",
                            /*error_code=*/NSURLErrorUnsupportedURL,
                            /*is_post=*/false, /*is_otr=*/false,
                            /*has_ssl_info=*/false)));
  NSString* data_html = @(kTestPageHTML);
  web_state()->LoadData([data_html dataUsingEncoding:NSUTF8StringEncoding],
                        @"text/html", echo_url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
  EXPECT_TRUE(
      test::WaitForWebViewContainingText(web_state(), kTextInTestPageHTML));

  web_state()->GetNavigationManager()->Reload(web::ReloadType::NORMAL, true);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));
  web_state()->GetNavigationManager()->Reload(web::ReloadType::NORMAL, true);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "Echo"));
}

INSTANTIATE_TEST_SUITE_P(
    ProgrammaticWebStateTest,
    WebStateTest,
    ::testing::Values(
        NavigationManagerChoice::TEST_LEGACY_NAVIGATION_MANAGER,
        NavigationManagerChoice::TEST_WK_BASED_NAVIGATION_MANAGER));

}  // namespace web
