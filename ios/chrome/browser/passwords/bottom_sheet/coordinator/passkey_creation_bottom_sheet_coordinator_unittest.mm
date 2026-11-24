// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/sync/test/test_sync_service.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "components/webauthn/ios/fake_ios_passkey_client.h"
#import "components/webauthn/ios/ios_passkey_client.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "components/webauthn/ios/passkey_test_util.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/model/test_favicon_loader.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class PasskeyCreationBottomSheetCoordinatorTest : public PlatformTest {
 protected:
  PasskeyCreationBottomSheetCoordinatorTest()
      : scoped_web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    animations_were_enabled_ = [UIView areAnimationsEnabled];
    [UIView setAnimationsEnabled:NO];

    mock_reauth_module_ = OCMProtocolMock(@protocol(ReauthenticationProtocol));
    OCMStub([mock_reauth_module_ canAttemptReauth]).andReturn(YES);

    TestProfileIOS::Builder builder;
    id<ReauthenticationProtocol> reauth_module = mock_reauth_module_;
    builder.AddTestingFactory(
        ReauthenticationServiceFactory::GetInstance(),
        base::BindOnce(
            [](id<ReauthenticationProtocol> module)
                -> std::unique_ptr<KeyedService> {
              return std::make_unique<ReauthenticationService>(module);
            },
            reauth_module));
    builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        base::BindOnce([]() -> std::unique_ptr<KeyedService> {
          return std::make_unique<TestFaviconLoader>();
        }));
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindOnce([]() -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
    profile_ = std::move(builder).Build();

    web::test::OverrideJavaScriptFeatures(
        profile_.get(), {webauthn::PasskeyJavaScriptFeature::GetInstance()});

    browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_passkey_handler_ =
        OCMStrictProtocolMock(@protocol(IOSPasskeyClientCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_passkey_handler_
                     forProtocol:@protocol(IOSPasskeyClientCommands)];

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto frame =
        web::FakeWebFrame::CreateMainWebFrame(GURL("https://example.com"));
    frame->set_browser_state(profile_.get());
    main_frame_ = frame.get();
    frames_manager->AddWebFrame(std::move(frame));
    web_state->SetWebFramesManager(
        webauthn::PasskeyJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld(),
        std::move(frames_manager));
    web_state->SetCurrentURL(GURL("https://example.com"));

    web_state_ = web_state.get();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::AtIndex(0).Activate());

    model_ = std::make_unique<webauthn::TestPasskeyModel>();
    auto client = std::make_unique<webauthn::FakeIOSPasskeyClient>();
    fake_client_ = client.get();
    webauthn::PasskeyTabHelper::CreateForWebState(web_state_, model_.get(),
                                                  /*password_store=*/nullptr,
                                                  std::move(client));

    base_view_controller_ = scoped_key_window_.Get().rootViewController;
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    // Wait for any presentation or dismissal transition to complete and drain
    // UI thread callbacks (such as `ExecuteWhenTransitionsComplete()`) while
    // `web::WebTaskEnvironment` is still alive.
    if (base_view_controller_.presentedViewController ||
        base_view_controller_.transitionCoordinator) {
      EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, ^bool {
            return base_view_controller_.presentedViewController == nil &&
                   base_view_controller_.transitionCoordinator == nil;
          }));
    }
    base::RunLoop().RunUntilIdle();
    base_view_controller_ = nil;
    main_frame_ = nullptr;
    web_state_ = nullptr;
    fake_client_ = nullptr;
    browser_.reset();
    profile_.reset();
    [UIView setAnimationsEnabled:animations_were_enabled_];
    PlatformTest::TearDown();
  }

  void StartCoordinatorWithValidRequest(
      device::UserVerificationRequirement user_verification_requirement =
          device::UserVerificationRequirement::kDiscouraged) {
    webauthn::RegistrationRequestParams params =
        webauthn::BuildRegistrationRequestParams({},
                                                 user_verification_requirement);
    request_id_ = params.RequestId();
    webauthn::PasskeyTabHelper::FromWebState(web_state_)
        ->HandleCreateRequestedEvent(std::move(params));

    coordinator_ = [[PasskeyCreationBottomSheetCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                       requestInfo:webauthn::IOSPasskeyClient::RequestInfo(
                                       web::kMainFakeFrameId, request_id_)];
    [coordinator_ start];
    if (base_view_controller_.transitionCoordinator) {
      ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
          base::test::ios::kWaitForUIElementTimeout, ^bool {
            return base_view_controller_.transitionCoordinator == nil;
          }));
    }
    base::RunLoop().RunUntilIdle();
  }

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient scoped_web_client_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<webauthn::TestPasskeyModel> model_;
  raw_ptr<web::FakeWebState> web_state_ = nullptr;
  raw_ptr<web::FakeWebFrame> main_frame_ = nullptr;
  raw_ptr<webauthn::FakeIOSPasskeyClient> fake_client_ = nullptr;
  id mock_reauth_module_;
  id mock_passkey_handler_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_;
  PasskeyCreationBottomSheetCoordinator* coordinator_;
  std::string request_id_;
  bool animations_were_enabled_ = true;
};

// Tests that interactively swiping down the passkey creation bottom sheet
// cancels the pending passkey request, dispatches dismissal, and logs
// `kDismissed` exactly once even after `-stop`.
TEST_F(PasskeyCreationBottomSheetCoordinatorTest,
       SwipeDismissCancelsRequestAndDismisses) {
  base::HistogramTester histogram_tester;
  StartCoordinatorWithValidRequest();
  ASSERT_TRUE(base_view_controller_.presentedViewController != nil);
  EXPECT_EQ(base_view_controller_.presentedViewController.presentationController
                .delegate,
            coordinator_);

  OCMExpect([mock_passkey_handler_ dismissPasskeyCreation]);

  id<UIAdaptivePresentationControllerDelegate> presentation_delegate =
      (id<UIAdaptivePresentationControllerDelegate>)coordinator_;
  [presentation_delegate
      presentationControllerDidDismiss:base_view_controller_
                                           .presentedViewController
                                           .presentationController];

  EXPECT_OCMOCK_VERIFY(mock_passkey_handler_);
  EXPECT_FALSE(webauthn::PasskeyTabHelper::FromWebState(web_state_)
                   ->ShouldPerformUserVerification(request_id_)
                   .has_value());
  EXPECT_NE(main_frame_->GetLastJavaScriptCall().find(u"rejectPasskeyRequest"),
            std::u16string::npos);

  // Stopping the coordinator afterwards must not log a second exit reason.
  [coordinator_ stop];
  coordinator_ = nil;
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kDismissed, 1);
}

// Tests that tapping the secondary action defers to the renderer, dispatches
// dismissal once, ignores subsequent double-taps, and logs `kSaveAnotherWay`.
TEST_F(PasskeyCreationBottomSheetCoordinatorTest,
       SecondaryActionDefersAndIgnoresDoubleTap) {
  base::HistogramTester histogram_tester;
  StartCoordinatorWithValidRequest();

  OCMExpect([mock_passkey_handler_ dismissPasskeyCreation]);

  id<ConfirmationAlertActionHandler> action_handler =
      (id<ConfirmationAlertActionHandler>)coordinator_;
  [action_handler confirmationAlertSecondaryAction];
  // A second tap should be ignored.
  [action_handler confirmationAlertSecondaryAction];

  EXPECT_OCMOCK_VERIFY(mock_passkey_handler_);
  EXPECT_NE(main_frame_->GetLastJavaScriptCall().find(u"deferToRenderer"),
            std::u16string::npos);

  [coordinator_ stop];
  coordinator_ = nil;
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kSaveAnotherWay, 1);
}

// Tests that tapping the primary action triggers passkey creation once,
// ignores subsequent double-taps, and logs `kCreatePasskey`.
TEST_F(PasskeyCreationBottomSheetCoordinatorTest,
       PrimaryActionCreatesPasskeyAndIgnoresDoubleTap) {
  base::HistogramTester histogram_tester;
  fake_client_->SetBiometricsEnabled(false);
  StartCoordinatorWithValidRequest();

  OCMExpect([mock_passkey_handler_ dismissPasskeyCreation]);

  id<ConfirmationAlertActionHandler> action_handler =
      (id<ConfirmationAlertActionHandler>)coordinator_;
  [action_handler confirmationAlertPrimaryAction];
  // A second tap should be ignored.
  [action_handler confirmationAlertPrimaryAction];

  EXPECT_OCMOCK_VERIFY(mock_passkey_handler_);
  EXPECT_TRUE(fake_client_->DidFetchTrustedVaultKeys());

  [coordinator_ stop];
  coordinator_ = nil;
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kCreatePasskey, 1);
}

// Tests that starting the coordinator for an unknown/expired request dismisses
// immediately without presenting the bottom sheet and logs `kInvalidRequest`.
TEST_F(PasskeyCreationBottomSheetCoordinatorTest,
       InvalidRequestDismissesWithoutPresenting) {
  base::HistogramTester histogram_tester;
  OCMExpect([mock_passkey_handler_ dismissPasskeyCreation])
      .andDo(^(NSInvocation*) {
        // Mirror `BrowserModalHost`, which synchronously stops and releases the
        // coordinator inside `-dismissPasskeyCreation`.
        [coordinator_ stop];
        coordinator_ = nil;
      });

  coordinator_ = [[PasskeyCreationBottomSheetCoordinator alloc]
      initWithBaseViewController:base_view_controller_
                         browser:browser_.get()
                     requestInfo:webauthn::IOSPasskeyClient::RequestInfo(
                                     web::kMainFakeFrameId,
                                     "non_existent_request_id")];
  [coordinator_ start];

  EXPECT_OCMOCK_VERIFY(mock_passkey_handler_);
  EXPECT_EQ(coordinator_, nil);
  EXPECT_EQ(base_view_controller_.presentedViewController, nil);
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kInvalidRequest, 1);
}

// Tests that stopping the coordinator without user interaction on the sheet
// logs `kDismissedExternally`.
TEST_F(PasskeyCreationBottomSheetCoordinatorTest,
       StopWithoutUserActionLogsDismissedExternally) {
  base::HistogramTester histogram_tester;
  StartCoordinatorWithValidRequest();

  [coordinator_ stop];
  coordinator_ = nil;

  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kDismissedExternally, 1);
}

// Tests that when the bottom sheet cannot be presented, `verifyPresentation()`
// defers to the renderer, dismisses, and logs `kCouldNotPresent`.
TEST_F(PasskeyCreationBottomSheetCoordinatorTest,
       PresentationFailureDefersAndLogsCouldNotPresent) {
  // Stub `-presentViewController:animated:completion:` to no-op, simulating
  // UIKit rejecting the presentation (e.g. another modal already being
  // presented).
  id mock_base_view_controller = OCMPartialMock(base_view_controller_);
  OCMStub([mock_base_view_controller presentViewController:[OCMArg any]
                                                  animated:YES
                                                completion:[OCMArg any]]);

  OCMExpect([mock_passkey_handler_ dismissPasskeyCreation]);

  base::HistogramTester histogram_tester;
  StartCoordinatorWithValidRequest();

  EXPECT_OCMOCK_VERIFY(mock_passkey_handler_);
  EXPECT_NE(main_frame_->GetLastJavaScriptCall().find(u"deferToRenderer"),
            std::u16string::npos);

  [coordinator_ stop];
  coordinator_ = nil;
  [mock_base_view_controller stopMocking];
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kCouldNotPresent, 1);
}

// Tests that calling `-stop` without user interaction cancels the pending
// passkey request.
TEST_F(PasskeyCreationBottomSheetCoordinatorTest, StopCancelsPendingRequest) {
  StartCoordinatorWithValidRequest();
  ASSERT_TRUE(webauthn::PasskeyTabHelper::FromWebState(web_state_)
                  ->ShouldPerformUserVerification(request_id_)
                  .has_value());

  [coordinator_ stop];
  coordinator_ = nil;

  EXPECT_FALSE(webauthn::PasskeyTabHelper::FromWebState(web_state_)
                   ->ShouldPerformUserVerification(request_id_)
                   .has_value());
  EXPECT_NE(main_frame_->GetLastJavaScriptCall().find(u"rejectPasskeyRequest"),
            std::u16string::npos);
}

// Tests that calling `-stop` multiple times is safe and clears the pending
// request once.
TEST_F(PasskeyCreationBottomSheetCoordinatorTest, StopCalledTwiceIsIdempotent) {
  StartCoordinatorWithValidRequest();
  webauthn::IOSPasskeyClient::RequestInfo request_info(web::kMainFakeFrameId,
                                                       request_id_);
  ASSERT_TRUE([coordinator_ hasPendingRequest:request_info]);

  [coordinator_ stop];
  EXPECT_FALSE([coordinator_ hasPendingRequest:request_info]);

  [coordinator_ stop];
  coordinator_ = nil;

  EXPECT_FALSE(webauthn::PasskeyTabHelper::FromWebState(web_state_)
                   ->ShouldPerformUserVerification(request_id_)
                   .has_value());
  EXPECT_NE(main_frame_->GetLastJavaScriptCall().find(u"rejectPasskeyRequest"),
            std::u16string::npos);
}

// Tests that cancelling a request via `HandleCancelRequestEvent()` (e.g.
// `AbortSignal`) synchronously stops the coordinator without sending a spurious
// `rejectPasskeyRequest` JavaScript call.
TEST_F(PasskeyCreationBottomSheetCoordinatorTest,
       AbortSignalCancelDoesNotSendSpuriousReject) {
  StartCoordinatorWithValidRequest();
  webauthn::IOSPasskeyClient::RequestInfo expected_request_info(
      web::kMainFakeFrameId, request_id_);
  ASSERT_TRUE([coordinator_ hasPendingRequest:expected_request_info]);

  bool cancel_callback_called = false;
  fake_client_->SetCancelPasskeyRequestCallback(base::BindLambdaForTesting(
      [this, &cancel_callback_called](
          webauthn::IOSPasskeyClient::RequestInfo request_info) {
        cancel_callback_called = true;
        // Mirror `BrowserModalHost`, which verifies `-hasPendingRequest:` and
        // synchronously stops and releases the coordinator.
        if ([coordinator_ hasPendingRequest:request_info]) {
          [coordinator_ stop];
          coordinator_ = nil;
        }
      }));

  webauthn::PasskeyTabHelper::FromWebState(web_state_)
      ->HandleCancelRequestEvent(expected_request_info);
  fake_client_->SetCancelPasskeyRequestCallback({});

  EXPECT_TRUE(cancel_callback_called);
  EXPECT_EQ(coordinator_, nil);
  EXPECT_TRUE(main_frame_->GetLastJavaScriptCall().empty());
}
