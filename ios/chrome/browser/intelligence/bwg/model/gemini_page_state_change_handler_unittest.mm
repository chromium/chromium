// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_state_change_handler.h"

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_future.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class GeminiPageStateChangeHandlerTest : public PlatformTest {
 protected:
  GeminiPageStateChangeHandlerTest() {
    // Register the pref used by the handler.
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kIOSBWGPageContentSetting, false);

    view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:view_controller_];

    handler_ = [[GeminiPageStateChangeHandler alloc]
        initWithPrefService:&pref_service_];
    [handler_ setBaseViewController:view_controller_];
  }

  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* view_controller_;
  GeminiPageStateChangeHandler* handler_;
};

// Tests that when the pref is already enabled, the completion callback is
// called with YES immediately.
TEST_F(GeminiPageStateChangeHandlerTest, TestPrefAlreadyEnabled) {
  pref_service_.SetBoolean(prefs::kIOSBWGPageContentSetting, true);

  base::test::TestFuture<BOOL> future;
  auto* future_ptr = &future;
  [handler_ requestPageContextSharingStatusWithCompletion:^(BOOL enabled) {
    future_ptr->SetValue(enabled);
  }];

  EXPECT_TRUE(future.Get());
  EXPECT_NSEQ(nil, view_controller_.presentedViewController);
}

// Tests that when the pref is disabled, an alert is presented.
TEST_F(GeminiPageStateChangeHandlerTest, TestPrefDisabledPresentsAlert) {
  pref_service_.SetBoolean(prefs::kIOSBWGPageContentSetting, false);

  base::test::TestFuture<BOOL> future;
  auto* future_ptr = &future;
  [handler_ requestPageContextSharingStatusWithCompletion:^(BOOL enabled) {
    future_ptr->SetValue(enabled);
  }];

  // The alert should be presented, so completion should not be called yet.
  EXPECT_FALSE(future.IsReady());

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return [view_controller_.presentedViewController
            isKindOfClass:[UIAlertController class]];
      }));
  UIAlertController* alert = base::apple::ObjCCastStrict<UIAlertController>(
      view_controller_.presentedViewController);

  EXPECT_EQ(2LU, alert.actions.count);
}

// Tests that accepting the alert enables the pref and calls completion with
// YES.
TEST_F(GeminiPageStateChangeHandlerTest, TestAlertAccept) {
  pref_service_.SetBoolean(prefs::kIOSBWGPageContentSetting, false);

  base::test::TestFuture<BOOL> future;
  auto* future_ptr = &future;
  [handler_ requestPageContextSharingStatusWithCompletion:^(BOOL enabled) {
    future_ptr->SetValue(enabled);
  }];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return [view_controller_.presentedViewController
            isKindOfClass:[UIAlertController class]];
      }));
  UIAlertController* alert = base::apple::ObjCCastStrict<UIAlertController>(
      view_controller_.presentedViewController);

  UIAlertAction* accept_action = nil;
  for (UIAlertAction* action in alert.actions) {
    if (action.style == UIAlertActionStyleDefault) {
      accept_action = action;
      break;
    }
  }
  ASSERT_NE(nil, accept_action);

  // Extract and call the handler. Note: Using KVC to access private "handler"
  // property is brittle but common in tests for UIAlertController.
  void (^action_handler)(UIAlertAction*) =
      [accept_action valueForKey:@"handler"];
  ASSERT_NE(nil, action_handler);
  action_handler(accept_action);

  EXPECT_TRUE(future.Get());
  EXPECT_TRUE(pref_service_.GetBoolean(prefs::kIOSBWGPageContentSetting));
}

// Tests that denying the alert calls completion with NO and does not enable the
// pref.
TEST_F(GeminiPageStateChangeHandlerTest, TestAlertDeny) {
  pref_service_.SetBoolean(prefs::kIOSBWGPageContentSetting, false);

  base::test::TestFuture<BOOL> future;
  auto* future_ptr = &future;
  [handler_ requestPageContextSharingStatusWithCompletion:^(BOOL enabled) {
    future_ptr->SetValue(enabled);
  }];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return [view_controller_.presentedViewController
            isKindOfClass:[UIAlertController class]];
      }));
  UIAlertController* alert = base::apple::ObjCCastStrict<UIAlertController>(
      view_controller_.presentedViewController);

  UIAlertAction* deny_action = nil;
  for (UIAlertAction* action in alert.actions) {
    if (action.style == UIAlertActionStyleCancel) {
      deny_action = action;
      break;
    }
  }
  ASSERT_NE(nil, deny_action);

  // Extract and call the handler. Note: Using KVC to access private "handler"
  // property is brittle but common in tests for UIAlertController.
  void (^action_handler)(UIAlertAction*) = [deny_action valueForKey:@"handler"];
  ASSERT_NE(nil, action_handler);
  action_handler(deny_action);

  EXPECT_FALSE(future.Get());
  EXPECT_FALSE(pref_service_.GetBoolean(prefs::kIOSBWGPageContentSetting));
}

}  // namespace
