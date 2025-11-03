// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_web_state_delegate.h"

#import "base/functional/callback_helpers.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/context_menu_params.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class MockWebStateDelegate : public web::WebStateDelegate {
 public:
  MOCK_METHOD(web::WebState*,
              CreateNewWebState,
              (web::WebState*, const GURL&, const GURL&, bool),
              (override));
  MOCK_METHOD(void, CloseWebState, (web::WebState*), (override));
  MOCK_METHOD(web::WebState*,
              OpenURLFromWebState,
              (web::WebState*, const web::WebState::OpenURLParams&),
              (override));
  MOCK_METHOD(void,
              ShowRepostFormWarningDialog,
              (web::WebState*,
               web::FormWarningType,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(web::JavaScriptDialogPresenter*,
              GetJavaScriptDialogPresenter,
              (web::WebState*),
              (override));
  MOCK_METHOD(void,
              HandlePermissionsDecisionRequest,
              (web::WebState*,
               NSArray<NSNumber*>*,
               web::WebStatePermissionDecisionHandler),
              (override));
  MOCK_METHOD(
      void,
      OnAuthRequired,
      (web::WebState*, NSURLProtectionSpace*, NSURLCredential*, AuthCallback),
      (override));
  MOCK_METHOD(void,
              ContextMenuConfiguration,
              (web::WebState*,
               const web::ContextMenuParams&,
               void (^)(UIContextMenuConfiguration*)),
              (override));
  MOCK_METHOD(void,
              ContextMenuWillCommitWithAnimator,
              (web::WebState*, id<UIContextMenuInteractionCommitAnimating>),
              (override));
  MOCK_METHOD(void,
              ShouldAllowCopy,
              (web::WebState*, base::OnceCallback<void(bool)>),
              (override));
};

}  // namespace

// Test fixture for ReaderModeWebStateDelegate.
class ReaderModeWebStateDelegateTest : public PlatformTest {
 protected:
  ReaderModeWebStateDelegateTest()
      : reader_mode_web_state_delegate_(nullptr, &mock_web_state_delegate_) {}

  testing::StrictMock<MockWebStateDelegate> mock_web_state_delegate_;
  ReaderModeWebStateDelegate reader_mode_web_state_delegate_;
};

// Tests that CreateNewWebState is forwarded.
TEST_F(ReaderModeWebStateDelegateTest, CreateNewWebStateForwarded) {
  EXPECT_CALL(mock_web_state_delegate_,
              CreateNewWebState(nullptr, GURL(), GURL(), false));
  reader_mode_web_state_delegate_.CreateNewWebState(nullptr, GURL(), GURL(),
                                                    false);
}

// Tests that GetJavaScriptDialogPresenter is not forwarded.
TEST_F(ReaderModeWebStateDelegateTest,
       GetJavaScriptDialogPresenterNotForwarded) {
  EXPECT_CALL(mock_web_state_delegate_, GetJavaScriptDialogPresenter(nullptr))
      .Times(0);
  reader_mode_web_state_delegate_.GetJavaScriptDialogPresenter(nullptr);
}

// Tests that ShowRepostFormWarningDialog is not forwarded.
TEST_F(ReaderModeWebStateDelegateTest,
       ShowRepostFormWarningDialogNotForwarded) {
  EXPECT_CALL(mock_web_state_delegate_,
              ShowRepostFormWarningDialog(testing::_, testing::_, testing::_))
      .Times(0);
  __block bool callback_called = false;
  reader_mode_web_state_delegate_.ShowRepostFormWarningDialog(
      nullptr, web::FormWarningType::kRepost,
      base::BindOnce([](bool* result, bool proceed) { *result = true; },
                     &callback_called));
  EXPECT_TRUE(callback_called);
}

// Tests that OnAuthRequired is not forwarded.
TEST_F(ReaderModeWebStateDelegateTest, OnAuthRequiredNotForwarded) {
  EXPECT_CALL(mock_web_state_delegate_,
              OnAuthRequired(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  __block bool callback_called = false;
  reader_mode_web_state_delegate_.OnAuthRequired(
      nullptr, nil, nil,
      base::BindOnce([](bool* result, NSString* username,
                        NSString* password) { *result = true; },
                     &callback_called));
  EXPECT_TRUE(callback_called);
}

// Tests that HandlePermissionsDecisionRequest is not forwarded.
TEST_F(ReaderModeWebStateDelegateTest,
       HandlePermissionsDecisionRequestNotForwarded) {
  EXPECT_CALL(mock_web_state_delegate_, HandlePermissionsDecisionRequest(
                                            testing::_, testing::_, testing::_))
      .Times(0);
  __block bool callback_called = false;
  web::WebStatePermissionDecisionHandler handler =
      ^(web::PermissionDecision decision) {
        callback_called = true;
      };
  reader_mode_web_state_delegate_.HandlePermissionsDecisionRequest(
      nullptr, @[], std::move(handler));
  EXPECT_TRUE(callback_called);
}

// Tests that ContextMenuConfiguration is forwarded.
TEST_F(ReaderModeWebStateDelegateTest, ContextMenuConfigurationForwarded) {
  EXPECT_CALL(mock_web_state_delegate_,
              ContextMenuConfiguration(nullptr, testing::_, testing::_));
  reader_mode_web_state_delegate_.ContextMenuConfiguration(
      nullptr, web::ContextMenuParams(),
      ^(UIContextMenuConfiguration*){
      });
}

// Tests that ContextMenuWillCommitWithAnimator is forwarded.
TEST_F(ReaderModeWebStateDelegateTest,
       ContextMenuWillCommitWithAnimatorForwarded) {
  EXPECT_CALL(mock_web_state_delegate_,
              ContextMenuWillCommitWithAnimator(nullptr, nil));
  reader_mode_web_state_delegate_.ContextMenuWillCommitWithAnimator(nullptr,
                                                                    nil);
}

// Tests that ShouldAllowCopy is forwarded.
TEST_F(ReaderModeWebStateDelegateTest, ShouldAllowCopyForwarded) {
  EXPECT_CALL(mock_web_state_delegate_, ShouldAllowCopy(nullptr, testing::_));
  reader_mode_web_state_delegate_.ShouldAllowCopy(nullptr, base::DoNothing());
}
