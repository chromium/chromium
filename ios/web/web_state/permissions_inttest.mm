// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/test/fakes/fake_web_state_delegate.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/test/fakes/crw_fake_wk_frame_info.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "ios/web/web_state/ui/crw_media_capture_permission_request.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::SpinRunLoopWithMinDelay;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Mocks WebStateObserver callbacks.
class WebStateObserverMock : public web::WebStateObserver {
 public:
  WebStateObserverMock() = default;

  WebStateObserverMock(const WebStateObserverMock&) = delete;
  WebStateObserverMock& operator=(const WebStateObserverMock&) = delete;

  MOCK_METHOD2(PermissionStateChanged, void(web::WebState*, web::Permission));
  void WebStateDestroyed(web::WebState* web_state) override {
    NOTREACHED_IN_MIGRATION();
  }
};

// Web client that simulates prerendering for testing purpose.
class TestPrerenderWebClient : public web::WebClient {
 public:
  TestPrerenderWebClient(web::WebTestWithWebState* test_case,
                         web::WebState* web_state)
      : test_case_(test_case), web_state_(web_state) {}

  // Like preload cancelling when attempting to show a prompt, this method
  // destroys the web state by closing the controller.
  void WillDisplayMediaCapturePermissionPrompt(
      web::WebState* web_state) const override {
    if (web_state == web_state_) {
      test_case_->DestroyWebState();
    }
  }

 private:
  raw_ptr<web::WebTestWithWebState> test_case_;
  raw_ptr<web::WebState> web_state_;
};

// Verifies that the current permission states matches expected.
ACTION_P3(VerifyPermissionState, web_state, permission, permission_state) {
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state->GetStateForPermission(permission), permission_state);
}

}  // namespace

namespace web {

// Tests fixture to test permissions handling for web state and its observer.
class PermissionsInttest : public WebTestWithWebController {
 public:
  void SetUp() override {
    WebTestWithWebController::SetUp();
    web_state()->AddObserver(&observer_);
    web_state()->SetDelegate(&delegate_);

    // Default setting; individual test cases can override.
    delegate_.SetShouldHandlePermissionDecision(true);

    // Set up test server.
    test_server_ = std::make_unique<net::EmbeddedTestServer>();
    test_server_->ServeFilesFromSourceDirectory(
        base::FilePath("ios/testing/data/http_server_files/permissions"));
    ASSERT_TRUE(test_server_->Start());
  }

  void TearDown() override {
    delegate_.ClearLastRequestedPermissions();
    if (web_state()) {
      web_state()->SetDelegate(nullptr);
      web_state()->RemoveObserver(&observer_);
    }
    WebTestWithWebController::TearDown();
  }

  // Returns whether the delegate has handled the request for expected
  // permissions.
  bool LastRequestedPermissionsMatchesPermissions(
      NSArray<NSNumber*>* expected) {
    NSArray<NSNumber*>* actual = delegate_.last_requested_permissions();
    if ([actual count] != [expected count]) {
      return false;
    }
    NSArray<NSNumber*>* expected_sorted =
        [expected sortedArrayUsingSelector:@selector(compare:)];
    NSArray<NSNumber*>* actual_sorted =
        [actual sortedArrayUsingSelector:@selector(compare:)];
    return [actual_sorted isEqualToArray:expected_sorted];
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
  testing::NiceMock<WebStateObserverMock> observer_;
  web::FakeWebStateDelegate delegate_;
};

// Disabling tests on real devices as it would wait for a user to respond to
// "ios_web_inttests Would Like to Access the Camera" prompt. This is currently
// not supported by gtest. Related logic and behaviors would be tested on real
// devices in integration tests.
#if TARGET_OS_SIMULATOR
  
namespace {

// This is the timeout used to wait for the WKUIDelegate's decision handler
// block of the to respond to permissions requests. The web state's permission
// states would only update after the decision handler blocks responds, so all
// checks should be ran after this timeout.
const base::TimeDelta kWebViewDecisionHandlingTimeout = base::Milliseconds(100);

constexpr std::string_view kSecureUrl = "https://www.chromium.org";
constexpr std::string_view kInsecureUrl = "http://www.chromium.org";

} // namespace

// Tests that web state observer gets invoked for camera only when the website
// only requests for camera permissions and changed via web_state() setter
// API afterwards.
TEST_F(PermissionsInttest,
       TestsThatPermissionStateChangedObserverInvokedForCameraOnly) {
  // TODO(crbug.com/342245057): Camera access is broken in the simulator on iOS
  // 17.5.
  if (@available(iOS 17.5, *)) {
    GTEST_SKIP() << "Test disabled on iOS 17.5.";
  }
  EXPECT_CALL(observer_, PermissionStateChanged(web_state(), PermissionCamera))
      .Times(testing::Exactly(2))
      .WillOnce(VerifyPermissionState(web_state(), PermissionCamera,
                                      PermissionStateAllowed));
  EXPECT_CALL(observer_,
              PermissionStateChanged(web_state(), PermissionMicrophone))
      .Times(0);
  delegate_.SetPermissionDecision(PermissionDecisionGrant);

  // Initial load.
  test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return LastRequestedPermissionsMatchesPermissions(@[ @(PermissionCamera) ]);
  }));
  SpinRunLoopWithMinDelay(kWebViewDecisionHandlingTimeout);
  EXPECT_EQ(web_state()->GetStateForPermission(PermissionCamera),
            PermissionStateAllowed);

  // Update permission through web state API.
  web_state()->SetStateForPermission(PermissionStateBlocked, PermissionCamera);
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, true, ^bool {
        return web_state()->GetStateForPermission(PermissionCamera) ==
               PermissionStateBlocked;
      }));
}

// Tests that web state observer gets invoked for microphone only when the
// website only requests for microphone permissions and changed via web_state()
// setter API afterwards.
TEST_F(PermissionsInttest,
       TestsThatPermissionStateChangedObserverInvokedForMicrophoneOnly) {
  EXPECT_CALL(observer_, PermissionStateChanged(web_state(), PermissionCamera))
      .Times(0);
  EXPECT_CALL(observer_,
              PermissionStateChanged(web_state(), PermissionMicrophone))
      .Times(testing::Exactly(2))
      .WillOnce(VerifyPermissionState(web_state(), PermissionMicrophone,
                                      PermissionStateAllowed));

  // Initial load.
  delegate_.SetPermissionDecision(PermissionDecisionGrant);
  test::LoadUrl(web_state(), test_server_->GetURL("/microphone_only.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return LastRequestedPermissionsMatchesPermissions(
        @[ @(PermissionMicrophone) ]);
  }));
  SpinRunLoopWithMinDelay(kWebViewDecisionHandlingTimeout);
  EXPECT_EQ(web_state()->GetStateForPermission(PermissionMicrophone),
            PermissionStateAllowed);

  // Update permission through web state API.
  web_state()->SetStateForPermission(PermissionStateNotAccessible,
                                     PermissionMicrophone);
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, true, ^bool {
        return web_state()->GetStateForPermission(PermissionMicrophone) ==
               PermissionStateNotAccessible;
      }));
}

// Tests that web state observer gets invoked for both camera and microphone,
// when both are requested by the web page and set via web_state() afterwards.
TEST_F(PermissionsInttest,
       TestsThatPermissionStateChangedObserverInvokedForCameraAndMicrophone) {
  // TODO(crbug.com/342245057): Camera access is broken in the simulator on iOS
  // 17.5.
  if (@available(iOS 17.5, *)) {
    GTEST_SKIP() << "Test disabled on iOS 17.5.";
  }
  EXPECT_CALL(observer_, PermissionStateChanged(web_state(), PermissionCamera))
      .Times(testing::Exactly(2))
      .WillOnce(VerifyPermissionState(web_state(), PermissionCamera,
                                      PermissionStateAllowed));
  EXPECT_CALL(observer_,
              PermissionStateChanged(web_state(), PermissionMicrophone))
      .Times(testing::Exactly(1))
      .WillOnce(VerifyPermissionState(web_state(), PermissionMicrophone,
                                      PermissionStateAllowed));

  // Initial load.
  delegate_.SetPermissionDecision(PermissionDecisionGrant);
  test::LoadUrl(web_state(),
                test_server_->GetURL("/camera_and_microphone.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return LastRequestedPermissionsMatchesPermissions(
        @[ @(PermissionCamera), @(PermissionMicrophone) ]);
  }));
  SpinRunLoopWithMinDelay(kWebViewDecisionHandlingTimeout);
  EXPECT_EQ(web_state()->GetStateForPermission(PermissionCamera),
            PermissionStateAllowed);
  EXPECT_EQ(web_state()->GetStateForPermission(PermissionMicrophone),
            PermissionStateAllowed);

  // Update permission through web state API.
  web_state()->SetStateForPermission(PermissionStateBlocked, PermissionCamera);
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, true, ^bool {
        return web_state()->GetStateForPermission(PermissionCamera) ==
               PermissionStateBlocked;
      }));
  EXPECT_FALSE(
      WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, true, ^bool {
        return web_state()->GetStateForPermission(PermissionMicrophone) !=
               PermissionStateAllowed;
      }));
}

// Tests that web state observer should not be invoked when permission is
// denied.
TEST_F(PermissionsInttest,
       TestsThatPermissionStateChangedObserverNotInvokedWhenPermissionDenied) {
  EXPECT_CALL(observer_, PermissionStateChanged(web_state(), PermissionCamera))
      .Times(0);

  delegate_.SetPermissionDecision(PermissionDecisionDeny);
  test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return LastRequestedPermissionsMatchesPermissions(@[ @(PermissionCamera) ]);
  }));
  SpinRunLoopWithMinDelay(kWebViewDecisionHandlingTimeout);
  EXPECT_EQ(web_state()->GetStateForPermission(PermissionCamera),
            PermissionStateNotAccessible);
  EXPECT_EQ([web_controller() ensureWebViewCreated].cameraCaptureState,
            WKMediaCaptureStateNone);
}

// Tests that permission could not be manually altered if it has never been
// granted by WKUIDelegate in the first place.
TEST_F(PermissionsInttest,
       TestsThatWebStateShouldNotAlterPermissionIfNotAccessible) {
  if (@available(iOS 17.0, *)) {
    // TODO(crbug.com/40921852): This crashes on iOS17, waiting for Apple fix.
    GTEST_SKIP() << "This crashes on iOS17, waiting for Apple fix.";
  }

  delegate_.SetPermissionDecision(PermissionDecisionDeny);
  test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return LastRequestedPermissionsMatchesPermissions(@[ @(PermissionCamera) ]);
  }));
  SpinRunLoopWithMinDelay(kWebViewDecisionHandlingTimeout);
  EXPECT_EQ(web_state()->GetStateForPermission(PermissionCamera),
            PermissionStateNotAccessible);

  // Update permission through web state API.
  web_state()->SetStateForPermission(PermissionStateAllowed, PermissionCamera);
  web_state()->SetStateForPermission(PermissionStateBlocked,
                                     PermissionMicrophone);
  // Neither permission should be changed.
  EXPECT_FALSE(
      WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
        // Camera permission asked but denied.
        BOOL camera_permission_changed =
            web_state()->GetStateForPermission(PermissionCamera) !=
            PermissionStateNotAccessible;
        // Microphone permission never asked.
        BOOL microphone_permission_changed =
            web_state()->GetStateForPermission(PermissionMicrophone) !=
            PermissionStateNotAccessible;

        return camera_permission_changed || microphone_permission_changed;
      }));
  EXPECT_EQ([web_controller() ensureWebViewCreated].cameraCaptureState,
            WKMediaCaptureStateNone);
  EXPECT_EQ([web_controller() ensureWebViewCreated].microphoneCaptureState,
            WKMediaCaptureStateNone);
}

// Tests that page reload resets permission states.
TEST_F(PermissionsInttest, TestsThatPageReloadResetsPermissionState) {
  if (@available(iOS 17.0, *)) {
    // TODO(crbug.com/40921852): This crashes on iOS17, waiting for Apple fix.
    GTEST_SKIP() << "This crashes on iOS17, waiting for Apple fix.";
  }

  // Initial load should allow permission.
  delegate_.SetPermissionDecision(PermissionDecisionGrant);
  test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return LastRequestedPermissionsMatchesPermissions(@[ @(PermissionCamera) ]);
  }));
  SpinRunLoopWithMinDelay(kWebViewDecisionHandlingTimeout);
  EXPECT_EQ(web_state()->GetStateForPermission(PermissionCamera),
            PermissionStateAllowed);

  // Reloading should reset permission.
  // Handler should be called again, and permission state should be
  // NotAccessible.
  delegate_.ClearLastRequestedPermissions();
  delegate_.SetPermissionDecision(PermissionDecisionDeny);
  web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                              /*check_for_repost=*/false);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return LastRequestedPermissionsMatchesPermissions(@[ @(PermissionCamera) ]);
  }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return web_state()->GetStateForPermission(PermissionCamera) ==
           PermissionStateNotAccessible;
  }));
  EXPECT_EQ([web_controller() ensureWebViewCreated].cameraCaptureState,
            WKMediaCaptureStateNone);
}

// Tests that the web state does not preserve permission states between
// navigations.
TEST_F(PermissionsInttest, TestsThatWebStateDoesNotPreservePermissionState) {
  if (@available(iOS 17.0, *)) {
    // TODO(crbug.com/40921852): This crashes on iOS17, waiting for Apple fix.
    GTEST_SKIP() << "This crashes on iOS17, waiting for Apple fix.";
  }

  // Initial load should allow permission.
  delegate_.SetPermissionDecision(PermissionDecisionGrant);
  test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return web_state()->GetStateForPermission(PermissionCamera) ==
           PermissionStateAllowed;
  }));
  EXPECT_TRUE(
      LastRequestedPermissionsMatchesPermissions(@[ @(PermissionCamera) ]));

  // Navigating to another page should reset permission after leaving the tab
  // running for a while. Handler should be called again, permission state
  // should be NotAccessible and the observer should NOT be invoked.
  delegate_.ClearLastRequestedPermissions();
  delegate_.SetPermissionDecision(PermissionDecisionDeny);
  test::LoadUrl(web_state(),
                test_server_->GetURL("/camera_and_microphone.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return !web_state()->IsLoading() &&
           LastRequestedPermissionsMatchesPermissions(
               @[ @(PermissionCamera), @(PermissionMicrophone) ]);
  }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return web_state()->GetStateForPermission(PermissionCamera) ==
               PermissionStateNotAccessible &&
           web_state()->GetStateForPermission(PermissionMicrophone) ==
               PermissionStateNotAccessible;
  }));
  EXPECT_EQ([web_controller() ensureWebViewCreated].cameraCaptureState,
            WKMediaCaptureStateNone);
  EXPECT_EQ([web_controller() ensureWebViewCreated].microphoneCaptureState,
            WKMediaCaptureStateNone);
}

// Tests that hitting "go back" and "go forward" resets permission states for
// pages with existing accessible permission states.
TEST_F(PermissionsInttest,
       TestsThatMovingBackwardOrForwardResetsPermissionState) {
  if (@available(iOS 17.0, *)) {
    // TODO(crbug.com/40921852): This crashes on iOS17, waiting for Apple fix.
    GTEST_SKIP() << "This crashes on iOS17, waiting for Apple fix.";
  }

  // Initial load for both pages should allow permission.
  delegate_.SetPermissionDecision(PermissionDecisionGrant);
  test::LoadUrl(web_state(), test_server_->GetURL("/microphone_only.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return web_state()->GetStateForPermission(PermissionMicrophone) ==
           PermissionStateAllowed;
  }));
  EXPECT_TRUE(
      LastRequestedPermissionsMatchesPermissions(@[ @(PermissionMicrophone) ]));

  test::LoadUrl(web_state(),
                test_server_->GetURL("/camera_and_microphone.html"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return web_state()->GetStateForPermission(PermissionCamera) ==
               PermissionStateAllowed &&
           web_state()->GetStateForPermission(PermissionMicrophone) ==
               PermissionStateAllowed;
  }));
  EXPECT_TRUE(LastRequestedPermissionsMatchesPermissions(
      @[ @(PermissionCamera), @(PermissionMicrophone) ]));

  // Update permission through web state API. To cover more cases, block
  // microphone on the second page.
  web_state()->SetStateForPermission(PermissionStateBlocked,
                                     PermissionMicrophone);
  EXPECT_TRUE(
      WaitUntilConditionOrTimeout(kWebViewDecisionHandlingTimeout, true, ^bool {
        return [web_controller() ensureWebViewCreated].microphoneCaptureState ==
               WKMediaCaptureStateMuted;
      }));

  // Permissions should be reset when you go backward or forward.

  // Note: There's currently an existing WebKit bug that WKUIDelegate method
  // `requestMediaCapturePermissionForOrigin:` would not be invoked when the
  // user hits backward/forward; instead, iOS sets them automatically to
  // WKMediaCaptureStateNone. The two following lines of code should be
  // uncommented when this is fixed.

  // delegate_.SetPermissionDecision(PermissionDecisionDeny);
  // handler_.decision = WKPermissionDecisionDeny;
  web_state()->GetNavigationManager()->GoBack();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return web_state()->GetStateForPermission(PermissionMicrophone) ==
           PermissionStateNotAccessible;
  }));
  EXPECT_EQ([web_controller() ensureWebViewCreated].microphoneCaptureState,
            WKMediaCaptureStateNone);

  web_state()->GetNavigationManager()->GoForward();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return web_state()->GetStateForPermission(PermissionCamera) ==
               PermissionStateNotAccessible &&
           web_state()->GetStateForPermission(PermissionMicrophone) ==
               PermissionStateNotAccessible;
  }));
  EXPECT_EQ([web_controller() ensureWebViewCreated].cameraCaptureState,
            WKMediaCaptureStateNone);
  EXPECT_EQ([web_controller() ensureWebViewCreated].microphoneCaptureState,
            WKMediaCaptureStateNone);
}

// Tests that closing tab before media capture request is handled denies
// permission.
TEST_F(PermissionsInttest, TestsThatClosingTabBeforeDecisionDeniesPermission) {
  // Set the permission decision to PermissionDecisionGrant first to eliminate
  // false positive test result, where the permission is erroneously declined
  // by the user/delegate instead of auto-declined by tab closing.
  delegate_.SetPermissionDecision(PermissionDecisionGrant);
  delegate_.SetShouldHandlePermissionDecision(false);

  // Initialize the decision to a value that should map to none of the
  // WKPermissionDecisions.
  __block NSInteger decision = -1;
  WKWebView* web_view = [web_controller() ensureWebViewCreated];
  id<WKUIDelegate> ui_delegate = web_view.UIDelegate;
  {
    // Fake a media capture permission request. Use an inner scope to allow
    // the request to be destroyed, simulating the closing of a tab.
    CRWMediaCapturePermissionRequest* request =
        [[CRWMediaCapturePermissionRequest alloc]
            initWithDecisionHandler:^(
                WKPermissionDecision wk_permission_decision) {
              decision = static_cast<NSInteger>(wk_permission_decision);
            }
                       onTaskRunner:base::SequencedTaskRunner::
                                        GetCurrentDefault()];
    request.presenter = (id<CRWMediaCapturePermissionPresenter>)ui_delegate;
    [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                       origin:GURL(kSecureUrl)];
  }

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return LastRequestedPermissionsMatchesPermissions(@[ @(PermissionCamera) ]);
  }));
  EXPECT_EQ(decision, static_cast<NSInteger>(WKPermissionDecisionDeny));
}

// Tests that media capture request is auto-declined when the tab is
// prerendering.
TEST_F(PermissionsInttest, TestsThatCancelllingPrerenderDeniesPermission) {
  ScopedTestingWebClient scoped_testing_web_client(
      std::make_unique<TestPrerenderWebClient>(this, web_state()));

  // Set the permission decision to PermissionDecisionGrant first to eliminate
  // false positive test result, where the permission is erroneously declined
  // by the user/delegate instead of auto-declined of prerender cancelling.
  delegate_.SetPermissionDecision(PermissionDecisionGrant);

  // Observer is not needed in this test case.
  web_state()->RemoveObserver(&observer_);

  // Initialize the decision to a value that should map to none of the
  // WKPermissionDecisions.
  __block NSInteger decision = -1;
  WKWebView* web_view = [web_controller() ensureWebViewCreated];
  id<WKUIDelegate> ui_delegate = web_view.UIDelegate;
  // Fake a media capture permission request.
  CRWFakeWKFrameInfo* frame_info = [[CRWFakeWKFrameInfo alloc] init];
  frame_info.mainFrame = YES;
  [ui_delegate webView:web_view
      requestMediaCapturePermissionForOrigin:frame_info.securityOrigin
                            initiatedByFrame:frame_info
                                        type:WKMediaCaptureTypeCamera
                             decisionHandler:^(
                                 WKPermissionDecision wk_permission_decision) {
                               decision = static_cast<NSInteger>(
                                   wk_permission_decision);
                             }];
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return decision == static_cast<NSInteger>(WKPermissionDecisionDeny);
  }));
  EXPECT_FALSE(web_state());
}

// Tests that permission is denied for non-secure origins.
TEST_F(PermissionsInttest, TestPermissionDeniedForNonSecureOrigin) {
  // Initialize the decision to a value that should map to none of the
  // WKPermissionDecisions.
  __block NSInteger decision = -1;
  WKWebView* web_view = [web_controller() ensureWebViewCreated];
  id<WKUIDelegate> ui_delegate = web_view.UIDelegate;

  // Fake a media capture permission request.
  CRWMediaCapturePermissionRequest* request = [[CRWMediaCapturePermissionRequest
      alloc]
      initWithDecisionHandler:^(WKPermissionDecision wk_permission_decision) {
        decision = static_cast<NSInteger>(wk_permission_decision);
      }
                 onTaskRunner:base::SequencedTaskRunner::GetCurrentDefault()];
  request.presenter = (id<CRWMediaCapturePermissionPresenter>)ui_delegate;
  [request displayPromptForMediaCaptureType:WKMediaCaptureTypeCamera
                                     origin:GURL(kInsecureUrl)];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, true, ^bool {
    return decision == static_cast<NSInteger>(WKPermissionDecisionDeny);
  }));
}

#endif  // TARGET_OS_SIMULATOR

}  // namespace web
