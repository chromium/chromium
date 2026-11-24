// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/password_manager_driver.h"
#import "components/sync/protocol/webauthn_credential_specifics.pb.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "components/webauthn/ios/fake_ios_passkey_client.h"
#import "components/webauthn/ios/ios_passkey_client.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "components/webauthn/ios/passkey_tab_helper.h"
#import "components/webauthn/ios/passkey_test_util.h"
#import "components/webauthn/ios/passkey_types.h"
#import "device/fido/public/public_key_credential_rp_entity.h"
#import "device/fido/public/public_key_credential_user_entity.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator_delegate.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/passkey_creation_bottom_sheet_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class PasskeyCreationBottomSheetMediatorTest : public PlatformTest {
 public:
  PasskeyCreationBottomSheetMediatorTest()
      : web_state_list_(&web_state_list_delegate_),
        scoped_web_client_(std::make_unique<web::FakeWebClient>()) {
    // Passkey registration is triggered from a webpage. However, in the tests,
    // passkey registration is triggered by directly calling PasskeyTabHelper's
    // member function.
    // There was a crash observed when PasskeyTabHelper called into Javascript
    // world when `SetJavaScriptFeatures` was used.
    // So, `OverrideJavaScriptFeatures` is used here to create a clean
    // environment.
    web::test::OverrideJavaScriptFeatures(
        &fake_browser_state_,
        {webauthn::PasskeyJavaScriptFeature::GetInstance()});

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(&fake_browser_state_);

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto frame =
        web::FakeWebFrame::CreateMainWebFrame(GURL("https://example.com"));
    frame->set_browser_state(&fake_browser_state_);
    frames_manager->AddWebFrame(std::move(frame));
    web_state->SetWebFramesManager(
        webauthn::PasskeyJavaScriptFeature::GetInstance()
            ->GetSupportedContentWorld(),
        std::move(frames_manager));
    web_state->SetCurrentURL(GURL("https://example.com"));

    web_state_ = web_state.get();
    web_state_list_.InsertWebState(std::move(web_state));
    web_state_list_.ActivateWebStateAt(0);

    model_ = std::make_unique<webauthn::TestPasskeyModel>();

    mock_delegate_ =
        OCMProtocolMock(@protocol(PasskeyCreationBottomSheetMediatorDelegate));
    mock_reauth_module_ = OCMProtocolMock(@protocol(ReauthenticationProtocol));
    OCMStub([mock_reauth_module_ canAttemptReauth]).andReturn(YES);

    auto client = std::make_unique<webauthn::FakeIOSPasskeyClient>();
    fake_client_ = client.get();
    webauthn::PasskeyTabHelper::CreateForWebState(web_state_, model_.get(),
                                                  /*password_store=*/nullptr,
                                                  std::move(client));
  }

 protected:
  std::unique_ptr<webauthn::TestPasskeyModel> model_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  raw_ptr<web::FakeWebState> web_state_;
  id mock_delegate_;
  id mock_reauth_module_;
  PasskeyCreationBottomSheetMediator* mediator_;
  web::ScopedTestingWebClient scoped_web_client_;
  web::FakeBrowserState fake_browser_state_;
  raw_ptr<webauthn::FakeIOSPasskeyClient> fake_client_;
};

// Tests that createPasskey starts creation immediately when user verification
// is not possible/required.
TEST_F(PasskeyCreationBottomSheetMediatorTest,
       CreatePasskeyNoUserVerification) {
  webauthn::PasskeyTabHelper* helper =
      webauthn::PasskeyTabHelper::FromWebState(web_state_);

  // Create a registration request.
  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});

  // Creates mediator with the correct request ID.
  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:webauthn::IOSPasskeyClient::RequestInfo(
                               "", params.RequestId())
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  helper->HandleCreateRequestedEvent(std::move(params));

  // Mock reauth module to say biometrics is NOT available.
  OCMStub([mock_reauth_module_ canAttemptReauthWithBiometrics]).andReturn(NO);
  fake_client_->SetBiometricsEnabled(false);
  // Expect NO reauth attempt.
  [[mock_reauth_module_ reject] attemptReauthWithLocalizedReason:[OCMArg any]
                                            canReusePreviousAuth:YES
                                                         handler:[OCMArg any]];

  base::HistogramTester histogram_tester;
  [mediator_ createPasskey];

  EXPECT_TRUE(fake_client_->DidFetchTrustedVaultKeys());
  [(OCMockObject*)mock_reauth_module_ verify];
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kCreatePasskey, 1);
}

// Tests that createPasskey attempts reauth when user verification is required
// and possible.
TEST_F(PasskeyCreationBottomSheetMediatorTest,
       CreatePasskeyWithUserVerification) {
  webauthn::PasskeyTabHelper* helper =
      webauthn::PasskeyTabHelper::FromWebState(web_state_);

  // Create a registration request.
  // Default is UserVerificationRequirement::kPreferred.
  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:webauthn::IOSPasskeyClient::RequestInfo(
                               "", params.RequestId())
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  helper->HandleCreateRequestedEvent(std::move(params));

  // Mock reauth module to say biometrics IS available.
  OCMStub([mock_reauth_module_ canAttemptReauthWithBiometrics]).andReturn(YES);

  // We expect a reauth attempt.
  // We capture the handler to invoke it.
  __block void (^completionHandler)(ReauthenticationResult);
  OCMExpect([mock_reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:[OCMArg checkWithBlock:^BOOL(id obj) {
                                 completionHandler = [obj copy];
                                 return YES;
                               }]]);

  [mediator_ createPasskey];

  // Verify reauth was attempted.
  [(OCMockObject*)mock_reauth_module_ verify];
  // Client should not have fetched keys yet.
  EXPECT_FALSE(fake_client_->DidFetchTrustedVaultKeys());

  // Trigger success.
  base::HistogramTester histogram_tester;
  completionHandler(ReauthenticationResult::kSuccess);

  // Now client should have fetched keys.
  EXPECT_TRUE(fake_client_->DidFetchTrustedVaultKeys());
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kCreatePasskey, 1);
}

// Tests that createPasskey handles reauth failure correctly.
TEST_F(PasskeyCreationBottomSheetMediatorTest, CreatePasskeyReauthFailure) {
  webauthn::PasskeyTabHelper* helper =
      webauthn::PasskeyTabHelper::FromWebState(web_state_);

  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:webauthn::IOSPasskeyClient::RequestInfo(
                               "", params.RequestId())
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  helper->HandleCreateRequestedEvent(std::move(params));

  // Mock reauth module to say biometrics IS available.
  OCMStub([mock_reauth_module_ canAttemptReauthWithBiometrics]).andReturn(YES);

  // We expect a reauth attempt.
  // We capture the handler to invoke it with failure.
  __block void (^completionHandler)(ReauthenticationResult);
  OCMExpect([mock_reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:[OCMArg checkWithBlock:^BOOL(id obj) {
                                 completionHandler = [obj copy];
                                 return YES;
                               }]]);

  [mediator_ createPasskey];

  // Verify reauth was attempted.
  [(OCMockObject*)mock_reauth_module_ verify];
  EXPECT_FALSE(fake_client_->DidFetchTrustedVaultKeys());

  // Expect dismissal on failure (as per current implementation logic in
  // mediator).
  [[mock_delegate_ expect] dismissPasskeyCreation];

  // Trigger failure.
  base::HistogramTester histogram_tester;
  completionHandler(ReauthenticationResult::kFailure);

  // Verify dismissal.
  [(OCMockObject*)mock_delegate_ verify];
  // Verify client did NOT fetch keys.
  EXPECT_FALSE(fake_client_->DidFetchTrustedVaultKeys());
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kReauthFailed, 1);
}

// Tests that createPasskey defers to the renderer and dismisses passkey
// creation when reauth cannot be attempted.
TEST_F(PasskeyCreationBottomSheetMediatorTest, CreatePasskeyReauthNotPossible) {
  webauthn::PasskeyTabHelper* helper =
      webauthn::PasskeyTabHelper::FromWebState(web_state_);

  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams(
          {}, device::UserVerificationRequirement::kRequired);

  // Re-create the mock for this test to avoid the default YES stub from setUp.
  mock_reauth_module_ = OCMProtocolMock(@protocol(ReauthenticationProtocol));
  OCMStub([mock_reauth_module_ canAttemptReauth]).andReturn(NO);

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:webauthn::IOSPasskeyClient::RequestInfo(
                               "", params.RequestId())
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  helper->HandleCreateRequestedEvent(std::move(params));

  // Expect dismissal on fallback to renderer.
  [[mock_delegate_ expect] dismissPasskeyCreation];

  base::HistogramTester histogram_tester;
  [mediator_ createPasskey];

  // Verify dismissal.
  [(OCMockObject*)mock_delegate_ verify];
  // Verify client did NOT fetch keys.
  EXPECT_FALSE(fake_client_->DidFetchTrustedVaultKeys());

  // Verify that the request was deferred to the renderer.
  web::WebFramesManager* frames_manager =
      webauthn::PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  web::FakeWebFrame* main_frame =
      static_cast<web::FakeWebFrame*>(frames_manager->GetMainWebFrame());
  EXPECT_NE(main_frame->GetLastJavaScriptCall().find(u"deferToRenderer"),
            std::u16string::npos);
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kReauthNotPossible, 1);
}

// Tests that switching to another WebState cancels the pending passkey request
// on the originating WebState and ends presentation.
TEST_F(PasskeyCreationBottomSheetMediatorTest,
       WebStateChangeCancelsPendingRequest) {
  webauthn::PasskeyTabHelper* helper =
      webauthn::PasskeyTabHelper::FromWebState(web_state_);

  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});
  std::string request_id = params.RequestId();

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:webauthn::IOSPasskeyClient::RequestInfo(
                               web::kMainFakeFrameId, request_id)
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  helper->HandleCreateRequestedEvent(std::move(params));
  ASSERT_TRUE(helper->ShouldPerformUserVerification(request_id).has_value());

  [[mock_delegate_ expect] endPresentation];

  base::HistogramTester histogram_tester;
  // Insert and activate a second WebState to trigger an active WebState change.
  auto second_web_state = std::make_unique<web::FakeWebState>();
  second_web_state->SetBrowserState(&fake_browser_state_);
  web_state_list_.InsertWebState(
      std::move(second_web_state),
      WebStateList::InsertionParams::AtIndex(1).Activate());

  [(OCMockObject*)mock_delegate_ verify];
  EXPECT_FALSE(helper->ShouldPerformUserVerification(request_id).has_value());

  web::WebFramesManager* frames_manager =
      webauthn::PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  web::FakeWebFrame* main_frame =
      static_cast<web::FakeWebFrame*>(frames_manager->GetMainWebFrame());
  EXPECT_NE(main_frame->GetLastJavaScriptCall().find(u"rejectPasskeyRequest"),
            std::u16string::npos);
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kDismissedExternally, 1);
}

// Tests that `-createPasskey` logs `kInvalidRequest` when the request is
// missing or expired.
TEST_F(PasskeyCreationBottomSheetMediatorTest,
       CreatePasskeyInvalidRequestLogsExitReason) {
  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:webauthn::IOSPasskeyClient::RequestInfo(
                               web::kMainFakeFrameId, "missing_request_id")
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  [[mock_delegate_ expect] dismissPasskeyCreation];

  base::HistogramTester histogram_tester;
  [mediator_ createPasskey];

  [(OCMockObject*)mock_delegate_ verify];
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kInvalidRequest, 1);
}

// Tests that `-logExitReason:` emits at most one sample per mediator instance.
TEST_F(PasskeyCreationBottomSheetMediatorTest, LogExitReasonEmitsAtMostOnce) {
  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:webauthn::IOSPasskeyClient::RequestInfo(
                               web::kMainFakeFrameId, "request_id")
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  base::HistogramTester histogram_tester;
  [mediator_
      logExitReason:PasskeyCreationBottomSheetExitReason::kSaveAnotherWay];
  [mediator_
      logExitReason:PasskeyCreationBottomSheetExitReason::kDismissedExternally];

  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kSaveAnotherWay, 1);
}

// Tests that cancelling passkey creation while reauthentication is in flight
// consumes the request and causes a subsequent reauthentication success to be
// ignored.
TEST_F(PasskeyCreationBottomSheetMediatorTest,
       CancelDuringInFlightReauthIgnoresReauthSuccess) {
  webauthn::PasskeyTabHelper* helper =
      webauthn::PasskeyTabHelper::FromWebState(web_state_);

  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});
  webauthn::IOSPasskeyClient::RequestInfo request_info(web::kMainFakeFrameId,
                                                       params.RequestId());

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:request_info
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  helper->HandleCreateRequestedEvent(std::move(params));
  OCMStub([mock_reauth_module_ canAttemptReauthWithBiometrics]).andReturn(YES);

  __block void (^completion_handler)(ReauthenticationResult);
  OCMExpect([mock_reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:[OCMArg checkWithBlock:^BOOL(id obj) {
                                 completion_handler = [obj copy];
                                 return YES;
                               }]]);

  [mediator_ createPasskey];
  [(OCMockObject*)mock_reauth_module_ verify];
  ASSERT_TRUE(completion_handler != nil);
  EXPECT_TRUE([mediator_ hasPendingRequest:request_info]);

  // Cancel while reauthentication is still pending.
  [mediator_ cancelPasskeyCreation];
  EXPECT_FALSE([mediator_ hasPendingRequest:request_info]);

  web::WebFramesManager* frames_manager =
      webauthn::PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  web::FakeWebFrame* main_frame =
      static_cast<web::FakeWebFrame*>(frames_manager->GetMainWebFrame());
  EXPECT_NE(main_frame->GetLastJavaScriptCall().find(u"rejectPasskeyRequest"),
            std::u16string::npos);

  // Completing reauthentication afterward should be a no-op.
  [[mock_delegate_ reject] dismissPasskeyCreation];
  completion_handler(ReauthenticationResult::kSuccess);

  [(OCMockObject*)mock_delegate_ verify];
  EXPECT_FALSE(fake_client_->DidFetchTrustedVaultKeys());
}

// Tests that calling `-createPasskey` while reauthentication is already in
// flight does not start a second reauthentication or consume the request twice.
TEST_F(PasskeyCreationBottomSheetMediatorTest,
       DuplicateCreatePasskeyDuringReauthIsIgnored) {
  webauthn::PasskeyTabHelper* helper =
      webauthn::PasskeyTabHelper::FromWebState(web_state_);

  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});
  webauthn::IOSPasskeyClient::RequestInfo request_info(web::kMainFakeFrameId,
                                                       params.RequestId());

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:request_info
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  helper->HandleCreateRequestedEvent(std::move(params));
  OCMStub([mock_reauth_module_ canAttemptReauthWithBiometrics]).andReturn(YES);

  __block int reauth_call_count = 0;
  __block void (^completion_handler)(ReauthenticationResult);
  OCMStub([mock_reauth_module_
      attemptReauthWithLocalizedReason:[OCMArg any]
                  canReusePreviousAuth:NO
                               handler:[OCMArg checkWithBlock:^BOOL(id obj) {
                                 ++reauth_call_count;
                                 completion_handler = [obj copy];
                                 return YES;
                               }]]);

  base::HistogramTester histogram_tester;
  [mediator_ createPasskey];
  [mediator_ createPasskey];
  EXPECT_EQ(reauth_call_count, 1);
  ASSERT_TRUE(completion_handler != nil);

  [[mock_delegate_ expect] dismissPasskeyCreation];
  completion_handler(ReauthenticationResult::kSuccess);

  [(OCMockObject*)mock_delegate_ verify];
  EXPECT_TRUE(fake_client_->DidFetchTrustedVaultKeys());
  EXPECT_FALSE([mediator_ hasPendingRequest:request_info]);
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kCreatePasskey, 1);
}

// Tests that once `-deferPasskeyCreationToRenderer` consumes the request,
// subsequent `-cancelPasskeyCreation` and `-deferPasskeyCreationToRenderer`
// calls are no-ops.
TEST_F(PasskeyCreationBottomSheetMediatorTest,
       DeferThenCancelDoesNotRejectRequest) {
  webauthn::PasskeyTabHelper* helper =
      webauthn::PasskeyTabHelper::FromWebState(web_state_);

  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});
  webauthn::IOSPasskeyClient::RequestInfo request_info(web::kMainFakeFrameId,
                                                       params.RequestId());

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:request_info
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  helper->HandleCreateRequestedEvent(std::move(params));
  ASSERT_TRUE([mediator_ hasPendingRequest:request_info]);

  [mediator_ deferPasskeyCreationToRenderer];
  EXPECT_FALSE([mediator_ hasPendingRequest:request_info]);

  [mediator_ cancelPasskeyCreation];
  [mediator_ deferPasskeyCreationToRenderer];

  web::WebFramesManager* frames_manager =
      webauthn::PasskeyJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state_);
  web::FakeWebFrame* main_frame =
      static_cast<web::FakeWebFrame*>(frames_manager->GetMainWebFrame());
  EXPECT_NE(main_frame->GetLastJavaScriptCall().find(u"deferToRenderer"),
            std::u16string::npos);
  EXPECT_EQ(main_frame->GetLastJavaScriptCall().find(u"rejectPasskeyRequest"),
            std::u16string::npos);
}

// Tests that `-disconnect` clears the pending request state.
TEST_F(PasskeyCreationBottomSheetMediatorTest, DisconnectClearsPendingRequest) {
  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});
  webauthn::IOSPasskeyClient::RequestInfo request_info(web::kMainFakeFrameId,
                                                       params.RequestId());

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:request_info
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  ASSERT_TRUE([mediator_ hasPendingRequest:request_info]);
  [mediator_ disconnect];
  EXPECT_FALSE([mediator_ hasPendingRequest:request_info]);
}

// Tests that `-createPasskey` clears the pending request and dismisses the
// bottom sheet when `webauthn::PasskeyTabHelper` is unavailable.
TEST_F(PasskeyCreationBottomSheetMediatorTest,
       CreatePasskeyWithoutPasskeyTabHelperDismisses) {
  // Insert and activate a `web::WebState` without a
  // `webauthn::PasskeyTabHelper`.
  auto web_state_without_helper = std::make_unique<web::FakeWebState>();
  web_state_without_helper->SetBrowserState(&fake_browser_state_);
  web_state_list_.InsertWebState(
      std::move(web_state_without_helper),
      WebStateList::InsertionParams::AtIndex(1).Activate());

  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});
  webauthn::IOSPasskeyClient::RequestInfo request_info(web::kMainFakeFrameId,
                                                       params.RequestId());

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
               requestInfo:request_info
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  [[mock_delegate_ expect] dismissPasskeyCreation];
  base::HistogramTester histogram_tester;
  [mediator_ createPasskey];

  [(OCMockObject*)mock_delegate_ verify];
  EXPECT_FALSE([mediator_ hasPendingRequest:request_info]);
  histogram_tester.ExpectUniqueSample(
      "IOS.PasskeyCreationBottomSheet.ExitReason",
      PasskeyCreationBottomSheetExitReason::kInvalidRequest, 1);
}
