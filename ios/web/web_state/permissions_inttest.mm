// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_wk_ui_handler.h"
#import "ios/web/web_state/ui/crw_wk_ui_handler_delegate.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;

namespace {

// This is the timeout used to wait for the WKUIDelegate's decision handler
// block of the to respond to permissions requests. The web state's permission
// states would only update after the decision handler blocks responds, so all
// checks should be ran after this timeout.
const CGFloat kWebViewDecisionHandlingTimeout = 0.1;

// Mocks WebStateObserver callbacks.
class WebStateObserverMock : public web::WebStateObserver {
 public:
  WebStateObserverMock() = default;

  WebStateObserverMock(const WebStateObserverMock&) = delete;
  WebStateObserverMock& operator=(const WebStateObserverMock&) = delete;

  MOCK_METHOD2(PermissionStateChanged, void(web::WebState*, web::Permission));
  void WebStateDestroyed(web::WebState* web_state) override { NOTREACHED(); }
};

// Verifies that the current permission states matches expected.
ACTION_P3(VerifyPermissionState, web_state, permission, permission_state) {
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state->GetStateForPermission(permission), permission_state);
}

}  // namespace

// Fake WKUIDelegate for WKWebView to override media permissions.
API_AVAILABLE(ios(15.0))
@interface FakeCRWWKUIHandler : CRWWKUIHandler

@property(nonatomic, assign) WKPermissionDecision decision;
@property(nonatomic, assign) BOOL decisionMade;

@end

@implementation FakeCRWWKUIHandler

// Method for WKUIDelegate to allow media permissions when requested.
- (void)webView:(WKWebView*)webView
    requestMediaCapturePermissionForOrigin:(WKSecurityOrigin*)origin
                          initiatedByFrame:(WKFrameInfo*)frame
                                      type:(WKMediaCaptureType)type
                           decisionHandler:
                               (void (^)(WKPermissionDecision decision))
                                   decisionHandler {
  decisionHandler(self.decision);
  // Adds timeout to make sure self.decisionMade is set after the decision
  // handler completes.
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               kWebViewDecisionHandlingTimeout * NSEC_PER_SEC),
                 dispatch_get_main_queue(), ^{
                   self.decisionMade = YES;
                 });
}

@end

namespace web {

// Tests fixture to test permissions handling for web state and its observer.
class PermissionsInttest : public WebTestWithWebController {
 public:
  void SetUp() override {
    WebTestWithWebState::SetUp();
    if (@available(iOS 15.0, *)) {
      // Turn on media permissions feature.
      scoped_feature_list_.InitWithFeatures(
          {features::kMediaPermissionsControl}, {});

      // Switch actual objects to fakes/mocks for testing purposes.
      handler_ = [[FakeCRWWKUIHandler alloc] init];
      handler_.delegate = (id<CRWWKUIHandlerDelegate>)web_controller();
      swizzler_ = std::make_unique<ScopedBlockSwizzler>(
          [CRWWebController class], NSSelectorFromString(@"UIHandler"), ^{
            return handler_;
          });

      web_state()->AddObserver(&observer_);

      // Set up test server.
      test_server_ = std::make_unique<net::EmbeddedTestServer>();
      test_server_->ServeFilesFromSourceDirectory(
          base::FilePath("ios/testing/data/http_server_files/permissions"));
      ASSERT_TRUE(test_server_->Start());
    }
  }

  void TearDown() override {
    web_state()->RemoveObserver(&observer_);
    WebTestWithWebState::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ScopedBlockSwizzler> swizzler_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
  testing::NiceMock<WebStateObserverMock> observer_;
  FakeCRWWKUIHandler* handler_ API_AVAILABLE(ios(15.0));
};

using base::test::ios::WaitUntilConditionOrTimeout;

// Disabling tests on real devices as it would wait for a user to respond to
// "ios_web_inttests Would Like to Access the Camera" prompt. This is currently
// not supported by gtest. Related logic and behaviors would be tested on real
// devices in integration tests.
#if TARGET_OS_SIMULATOR

// Tests that web state observer gets invoked for camera only when the website
// only requests for camera permissions and changed via web_state() setter
// API afterwards.
TEST_F(PermissionsInttest,
       TestsThatPermissionStateChangedObserverInvokedForCameraOnly) {
  if (@available(iOS 15.0, *)) {
    EXPECT_CALL(observer_,
                PermissionStateChanged(web_state(), PermissionCamera))
        .Times(testing::Exactly(2))
        .WillOnce(VerifyPermissionState(web_state(), PermissionCamera,
                                        PermissionStateAllowed));
    EXPECT_CALL(observer_,
                PermissionStateChanged(web_state(), PermissionMicrophone))
        .Times(0);

    // Initial load.
    handler_.decision = WKPermissionDecisionGrant;
    test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return handler_.decisionMade;
    }));

    // Update permission through web state API.
    web_state()->SetStateForPermission(PermissionStateBlocked,
                                       PermissionCamera);
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return web_state()->GetStateForPermission(PermissionCamera) ==
             PermissionStateBlocked;
    }));
  }
}

// Tests that web state observer gets invoked for microphone only when the
// website only requests for microphone permissions and changed via web_state()
// setter API afterwards.
TEST_F(PermissionsInttest,
       TestsThatPermissionStateChangedObserverInvokedForMicrophoneOnly) {
  if (@available(iOS 15.0, *)) {
    EXPECT_CALL(observer_,
                PermissionStateChanged(web_state(), PermissionCamera))
        .Times(0);
    EXPECT_CALL(observer_,
                PermissionStateChanged(web_state(), PermissionMicrophone))
        .Times(testing::Exactly(2))
        .WillOnce(VerifyPermissionState(web_state(), PermissionMicrophone,
                                        PermissionStateAllowed));

    // Initial load.
    handler_.decision = WKPermissionDecisionGrant;
    test::LoadUrl(web_state(), test_server_->GetURL("/microphone_only.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return handler_.decisionMade;
    }));

    // Update permission through web state API.
    web_state()->SetStateForPermission(PermissionStateNotAccessible,
                                       PermissionMicrophone);
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return web_state()->GetStateForPermission(PermissionMicrophone) ==
             PermissionStateNotAccessible;
    }));
  }
}

// Tests that web state observer gets invoked for both camera and microphone,
// when both are requested by the web page and set via web_state() afterwards.
TEST_F(PermissionsInttest,
       TestsThatPermissionStateChangedObserverInvokedForCameraAndMicrophone) {
  if (@available(iOS 15.0, *)) {
    EXPECT_CALL(observer_,
                PermissionStateChanged(web_state(), PermissionCamera))
        .Times(testing::Exactly(2))
        .WillOnce(VerifyPermissionState(web_state(), PermissionCamera,
                                        PermissionStateAllowed));
    EXPECT_CALL(observer_,
                PermissionStateChanged(web_state(), PermissionMicrophone))
        .Times(testing::Exactly(1))
        .WillOnce(VerifyPermissionState(web_state(), PermissionMicrophone,
                                        PermissionStateAllowed));

    // Initial load.
    handler_.decision = WKPermissionDecisionGrant;
    test::LoadUrl(web_state(),
                  test_server_->GetURL("/camera_and_microphone.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return handler_.decisionMade;
    }));

    // Only block one of them (camera in this case).
    web_state()->SetStateForPermission(PermissionStateBlocked,
                                       PermissionCamera);
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return web_state()->GetStateForPermission(PermissionCamera) ==
             PermissionStateBlocked;
    }));
    EXPECT_EQ(web_state()->GetStateForPermission(PermissionMicrophone),
              PermissionStateAllowed);
  }
}

// Tests that web state observer should not be invoked when permission is
// denied.
TEST_F(PermissionsInttest,
       TestsThatPermissionStateChangedObserverNotInvokedWhenPermissionDenied) {
  if (@available(iOS 15.0, *)) {
    EXPECT_CALL(observer_,
                PermissionStateChanged(web_state(), PermissionCamera))
        .Times(0);

    handler_.decision = WKPermissionDecisionDeny;
    test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return handler_.decisionMade;
    }));
    EXPECT_EQ([web_controller() ensureWebViewCreated].cameraCaptureState,
              WKMediaCaptureStateNone);
  }
}

// Tests that permission could not be manually altered if it has never been
// granted by WKUIDelegate in the first place.
TEST_F(PermissionsInttest,
       TestsThatWebStateShouldNotAlterPermissionIfNotAccessible) {
  if (@available(iOS 15.0, *)) {
    handler_.decision = WKPermissionDecisionDeny;
    test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return handler_.decisionMade;
    }));
    web_state()->SetStateForPermission(PermissionStateAllowed,
                                       PermissionCamera);
    web_state()->SetStateForPermission(PermissionStateBlocked,
                                       PermissionMicrophone);
    // Neither permission should be changed.
    EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      // Camera permission asked but denied.
      BOOL cameraPermissionChanged =
          web_state()->GetStateForPermission(PermissionCamera) !=
          PermissionStateNotAccessible;
      // Microphone permission never asked.
      BOOL microphonePermissionChanged =
          web_state()->GetStateForPermission(PermissionMicrophone) !=
          PermissionStateNotAccessible;

      return cameraPermissionChanged || microphonePermissionChanged;
    }));
    EXPECT_EQ([web_controller() ensureWebViewCreated].cameraCaptureState,
              WKMediaCaptureStateNone);
    EXPECT_EQ([web_controller() ensureWebViewCreated].microphoneCaptureState,
              WKMediaCaptureStateNone);
  }
}

// Tests that page reload resets permission states.
TEST_F(PermissionsInttest, TestsThatPageReloadResetsPermissionState) {
  if (@available(iOS 15.0, *)) {
    // Initial load should allow permission.
    handler_.decision = WKPermissionDecisionGrant;
    test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return web_state()->GetStateForPermission(PermissionCamera) ==
             PermissionStateAllowed;
    }));

    // Reload should reset permission. Handler should be called again, and
    // permission state should be NotAccessible.
    handler_.decisionMade = NO;
    handler_.decision = WKPermissionDecisionDeny;
    web_state()->GetNavigationManager()->Reload(ReloadType::NORMAL,
                                                /*check_for_repost=*/false);
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return web_state()->GetStateForPermission(PermissionCamera) ==
             PermissionStateNotAccessible;
    }));
    EXPECT_EQ([web_controller() ensureWebViewCreated].cameraCaptureState,
              WKMediaCaptureStateNone);
  }
}

// Tests that the web state does not preserve permission states between
// navigations.
TEST_F(PermissionsInttest, TestsThatWebStateDoesNotPreservePermissionState) {
  if (@available(iOS 15.0, *)) {
    // Initial load should allow permission.
    handler_.decision = WKPermissionDecisionGrant;
    test::LoadUrl(web_state(), test_server_->GetURL("/camera_only.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return web_state()->GetStateForPermission(PermissionCamera) ==
             PermissionStateAllowed;
    }));

    // Navigating to another page should reset permission. Handler should be
    // called again, permission state should be NotAccessible and the observer
    // should NOT be invoked.
    handler_.decisionMade = NO;
    handler_.decision = WKPermissionDecisionDeny;
    test::LoadUrl(web_state(),
                  test_server_->GetURL("/camera_and_microphone.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return handler_.decisionMade &&
             web_state()->GetStateForPermission(PermissionCamera) ==
                 PermissionStateNotAccessible;
    }));
    EXPECT_EQ([web_controller() ensureWebViewCreated].cameraCaptureState,
              WKMediaCaptureStateNone);
    EXPECT_EQ([web_controller() ensureWebViewCreated].microphoneCaptureState,
              WKMediaCaptureStateNone);
  }
}

// Tests that hitting "go back" and "go forward" resets permission states for
// pages with existing accessible permission states.
TEST_F(PermissionsInttest,
       TestsThatMovingBackwardOrForwardResetsPermissionState) {
  if (@available(iOS 15.0, *)) {
    // Initial load for both pages should allow permission.
    handler_.decision = WKPermissionDecisionGrant;
    test::LoadUrl(web_state(), test_server_->GetURL("/microphone_only.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return web_state()->GetStateForPermission(PermissionMicrophone) ==
             PermissionStateAllowed;
    }));
    test::LoadUrl(web_state(),
                  test_server_->GetURL("/camera_and_microphone.html"));
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return web_state()->GetStateForPermission(PermissionCamera) ==
                 PermissionStateAllowed &&
             web_state()->GetStateForPermission(PermissionMicrophone) ==
                 PermissionStateAllowed;
    }));
    // To cover more cases, block microphone on the second page.
    web_state()->SetStateForPermission(PermissionStateBlocked,
                                       PermissionMicrophone);
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return [web_controller() ensureWebViewCreated].microphoneCaptureState ==
             WKMediaCaptureStateMuted;
    }));

    // Permissions should be reset when you go backward or forward.

    // Note: There's currently an existing WebKit bug that WKUIDelegate method
    // |requestMediaCapturePermissionForOrigin:| would not be invoked when the
    // user hits backward/forward; instead, iOS sets them automatically to
    // WKMediaCaptureStateNone. The two following lines of code should be
    // uncommented when this is fixed.

    // handler_.decisionMade = NO;
    // handler_.decision = WKPermissionDecisionDeny;
    web_state()->GetNavigationManager()->GoBack();
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
      return web_state()->GetStateForPermission(PermissionMicrophone) ==
             PermissionStateNotAccessible;
    }));
    EXPECT_EQ([web_controller() ensureWebViewCreated].microphoneCaptureState,
              WKMediaCaptureStateNone);

    web_state()->GetNavigationManager()->GoForward();
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^bool {
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
}
#endif  // TARGET_OS_SIMULATOR

}  // namespace web
