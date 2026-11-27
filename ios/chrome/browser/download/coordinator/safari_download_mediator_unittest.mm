// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/safari_download_mediator.h"

#import <memory>
#import <optional>

#import "ios/chrome/browser/download/coordinator/safari_download_mediator_delegate.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper.h"
#import "ios/chrome/browser/download/model/safari_download_tab_helper_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class SafariDownloadMediatorTest : public PlatformTest {
 public:
  SafariDownloadMediatorTest()
      : profile_(TestProfileIOS::Builder().Build()), browser_(profile_.get()) {
    mediator_ = [[SafariDownloadMediator alloc]
        initWithActiveWebState:web_state_list()->GetActiveWebState()];
    delegate_mock_ =
        OCMStrictProtocolMock(@protocol(SafariDownloadMediatorDelegate));
    tab_helper_delegate_mock_ =
        OCMStrictProtocolMock(@protocol(SafariDownloadTabHelperDelegate));
    mediator_.delegate = delegate_mock_;
    mediator_.tabHelperDelegate = tab_helper_delegate_mock_;
    dependency_installer_bridge_.StartObserving(mediator_, &browser_);
  }

  ~SafariDownloadMediatorTest() override {
    dependency_installer_bridge_.StopObserving();
    [mediator_ disconnect];
  }

 protected:
  // Returns the browser's `WebStateList`.
  WebStateList* web_state_list() { return browser_.GetWebStateList(); }

  // Inserts and activates a new `FakeWebState` with `SafariDownloadTabHelper`.
  web::FakeWebState* InsertActiveWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web::FakeWebState* web_state_ptr = web_state.get();
    SafariDownloadTabHelper::CreateForWebState(web_state_ptr);
    web_state_list()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
    return web_state_ptr;
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  TestBrowser browser_;
  TabsDependencyInstallerBridge dependency_installer_bridge_;
  SafariDownloadMediator* mediator_;
  OCMockObject<SafariDownloadMediatorDelegate>* delegate_mock_;
  OCMockObject<SafariDownloadTabHelperDelegate>* tab_helper_delegate_mock_;
};

// Tests dismissal when the active WebState starts cross-document navigation.
TEST_F(SafariDownloadMediatorTest, DismissOnNavigationStart) {
  web::FakeWebState* web_state = InsertActiveWebState();

  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:web_state]);
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  web_state->OnNavigationStarted(&context);

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests no dismissal on same-document navigation start.
TEST_F(SafariDownloadMediatorTest, DoNotDismissOnSameDocumentNavigationStart) {
  web::FakeWebState* web_state = InsertActiveWebState();

  web::FakeNavigationContext context;
  context.SetIsSameDocument(true);
  web_state->OnNavigationStarted(&context);

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests dismissal when the active WebState finishes committed cross-document
// navigation.
TEST_F(SafariDownloadMediatorTest, DismissOnNavigationFinish) {
  web::FakeWebState* web_state = InsertActiveWebState();

  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:web_state]);
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state->OnNavigationFinished(&context);

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests no dismissal on same-document navigation finish.
TEST_F(SafariDownloadMediatorTest, DoNotDismissOnSameDocumentNavigationFinish) {
  web::FakeWebState* web_state = InsertActiveWebState();

  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(true);
  web_state->OnNavigationFinished(&context);

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests no dismissal on uncommitted navigation finish.
TEST_F(SafariDownloadMediatorTest, DoNotDismissOnUncommittedNavigationFinish) {
  web::FakeWebState* web_state = InsertActiveWebState();

  web::FakeNavigationContext context;
  context.SetHasCommitted(false);
  context.SetIsSameDocument(false);
  web_state->OnNavigationFinished(&context);

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests dismissal when the active WebState is hidden.
TEST_F(SafariDownloadMediatorTest, DismissOnWebStateHidden) {
  web::FakeWebState* web_state = InsertActiveWebState();
  web_state->WasShown();

  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:web_state]);
  web_state->WasHidden();

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests dismissal when the active WebState changes.
TEST_F(SafariDownloadMediatorTest, DismissOnActiveWebStateChange) {
  web::FakeWebState* web_state1 = InsertActiveWebState();

  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:web_state1]);
  InsertActiveWebState();

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests dismissal when the last active WebState closes, including reentrant
// disconnect during dismissal.
TEST_F(SafariDownloadMediatorTest, DismissOnLastActiveWebStateClosed) {
  web::FakeWebState* web_state = InsertActiveWebState();

  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:web_state])
      .andDo(^(NSInvocation* invocation) {
        [mediator_ disconnect];
      });
  web_state_list()->CloseWebStateAt(0, WebStateList::ClosingReason::kDefault);

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests dismissal when the active WebState is closed by user action.
TEST_F(SafariDownloadMediatorTest, DismissOnWebStateClosedByUserAction) {
  web::FakeWebState* opener = InsertActiveWebState();

  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:opener]);
  web::FakeWebState* popup = InsertActiveWebState();

  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:popup]);
  web_state_list()->CloseWebStateAt(1,
                                    WebStateList::ClosingReason::kUserAction);

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests dismissal when the active WebState is detached.
TEST_F(SafariDownloadMediatorTest, DismissOnActiveWebStateDetached) {
  web::FakeWebState* web_state = InsertActiveWebState();

  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:web_state]);
  std::unique_ptr<web::WebState> detached =
      web_state_list()->DetachWebStateAt(0);

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests dismissal when the active WebState is replaced.
TEST_F(SafariDownloadMediatorTest, DismissOnActiveWebStateReplaced) {
  web::FakeWebState* web_state1 = InsertActiveWebState();

  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:web_state1]);
  web_state_list()->ReplaceWebStateAt(0, std::make_unique<web::FakeWebState>());

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that background WebState navigation and hiding do not dismiss.
TEST_F(SafariDownloadMediatorTest, BackgroundWebStateEventsDoNotDismiss) {
  InsertActiveWebState();

  auto background_web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* background_ptr = background_web_state.get();
  background_ptr->WasShown();
  web_state_list()->InsertWebState(std::move(background_web_state),
                                   WebStateList::InsertionParams::AtIndex(1));

  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  context.SetHasCommitted(true);
  background_ptr->OnNavigationStarted(&context);
  background_ptr->OnNavigationFinished(&context);
  background_ptr->WasHidden();

  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that `SafariDownloadTabHelper` delegates are installed on insert and
// replace, and cleared on replacement and `StopObserving()`.
TEST_F(SafariDownloadMediatorTest, InstallsAndClearsTabHelperDelegates) {
  web::FakeWebState* web_state1 = InsertActiveWebState();
  EXPECT_EQ(static_cast<id<SafariDownloadTabHelperDelegate>>(
                tab_helper_delegate_mock_),
            SafariDownloadTabHelper::FromWebState(web_state1)->delegate());

  auto web_state2 = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state2_ptr = web_state2.get();
  SafariDownloadTabHelper::CreateForWebState(web_state2_ptr);
  OCMExpect([delegate_mock_ safariDownloadMediator:mediator_
                     shouldDismissAlertForWebState:web_state1]);
  std::unique_ptr<web::WebState> replaced =
      web_state_list()->ReplaceWebStateAt(0, std::move(web_state2));
  EXPECT_EQ(nil,
            SafariDownloadTabHelper::FromWebState(replaced.get())->delegate());
  EXPECT_EQ(static_cast<id<SafariDownloadTabHelperDelegate>>(
                tab_helper_delegate_mock_),
            SafariDownloadTabHelper::FromWebState(web_state2_ptr)->delegate());

  dependency_installer_bridge_.StopObserving();
  EXPECT_EQ(nil,
            SafariDownloadTabHelper::FromWebState(web_state2_ptr)->delegate());
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}

// Tests that an already-active `WebState` passed at initialization is observed
// and receives its `SafariDownloadTabHelper` delegate when observation starts.
TEST_F(SafariDownloadMediatorTest, ObservesInitialActiveWebState) {
  TestBrowser local_browser(profile_.get());
  auto web_state = std::make_unique<web::FakeWebState>();
  web::FakeWebState* web_state_ptr = web_state.get();
  SafariDownloadTabHelper::CreateForWebState(web_state_ptr);
  local_browser.GetWebStateList()->InsertWebState(
      std::move(web_state),
      WebStateList::InsertionParams::Automatic().Activate());

  OCMockObject<SafariDownloadMediatorDelegate>* local_delegate_mock =
      OCMStrictProtocolMock(@protocol(SafariDownloadMediatorDelegate));
  OCMockObject<SafariDownloadTabHelperDelegate>* local_tab_helper_delegate =
      OCMStrictProtocolMock(@protocol(SafariDownloadTabHelperDelegate));
  SafariDownloadMediator* local_mediator =
      [[SafariDownloadMediator alloc] initWithActiveWebState:web_state_ptr];
  local_mediator.delegate = local_delegate_mock;
  local_mediator.tabHelperDelegate = local_tab_helper_delegate;

  TabsDependencyInstallerBridge local_bridge;
  local_bridge.StartObserving(local_mediator, &local_browser);
  EXPECT_EQ(static_cast<id<SafariDownloadTabHelperDelegate>>(
                local_tab_helper_delegate),
            SafariDownloadTabHelper::FromWebState(web_state_ptr)->delegate());

  OCMExpect([local_delegate_mock safariDownloadMediator:local_mediator
                          shouldDismissAlertForWebState:web_state_ptr]);
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);
  web_state_ptr->OnNavigationStarted(&context);

  local_bridge.StopObserving();
  [local_mediator disconnect];
  EXPECT_EQ(nil,
            SafariDownloadTabHelper::FromWebState(web_state_ptr)->delegate());
  EXPECT_OCMOCK_VERIFY(local_delegate_mock);
}

// Tests that destroying an observed active `WebState` directly notifies the
// delegate and clears the observation.
TEST_F(SafariDownloadMediatorTest, DismissOnActiveWebStateDestroyed) {
  std::optional<web::FakeWebState> web_state(std::in_place);
  web::FakeWebState* web_state_ptr = &*web_state;

  OCMockObject<SafariDownloadMediatorDelegate>* local_delegate_mock =
      OCMStrictProtocolMock(@protocol(SafariDownloadMediatorDelegate));
  SafariDownloadMediator* local_mediator =
      [[SafariDownloadMediator alloc] initWithActiveWebState:web_state_ptr];
  local_mediator.delegate = local_delegate_mock;

  OCMExpect([local_delegate_mock safariDownloadMediator:local_mediator
                          shouldDismissAlertForWebState:web_state_ptr]);
  web_state.reset();

  [local_mediator disconnect];
  EXPECT_OCMOCK_VERIFY(local_delegate_mock);
}

// Tests that `TabsDependencyInstalling` callbacks after `-disconnect` are
// safely ignored.
TEST_F(SafariDownloadMediatorTest, IgnoreActiveWebStateChangeAfterDisconnect) {
  InsertActiveWebState();
  [mediator_ disconnect];

  web::FakeWebState* second_web_state = InsertActiveWebState();
  EXPECT_EQ(
      nil, SafariDownloadTabHelper::FromWebState(second_web_state)->delegate());
  EXPECT_OCMOCK_VERIFY(delegate_mock_);
}
