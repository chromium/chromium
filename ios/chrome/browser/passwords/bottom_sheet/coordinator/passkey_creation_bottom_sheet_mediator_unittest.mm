// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
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

    auto client = std::make_unique<webauthn::FakeIOSPasskeyClient>(web_state_);
    fake_client_ = client.get();
    webauthn::PasskeyTabHelper::CreateForWebState(web_state_, model_.get(),
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
                 requestID:params.RequestId()
          accountForSaving:@"test@example.com"
              reauthModule:mock_reauth_module_
                  delegate:mock_delegate_];

  helper->HandleCreateRequestedEvent(std::move(params));

  // Mock reauth module to say biometrics is NOT available.
  OCMStub([mock_reauth_module_ canAttemptReauthWithBiometrics]).andReturn(NO);
  // Expect NO reauth attempt.
  [[mock_reauth_module_ reject] attemptReauthWithLocalizedReason:[OCMArg any]
                                            canReusePreviousAuth:YES
                                                         handler:[OCMArg any]];

  [mediator_ createPasskey];

  EXPECT_TRUE(fake_client_->DidFetchKeys());
  [(OCMockObject*)mock_reauth_module_ verify];
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
                 requestID:params.RequestId()
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
                  canReusePreviousAuth:YES
                               handler:[OCMArg checkWithBlock:^BOOL(id obj) {
                                 completionHandler = [obj copy];
                                 return YES;
                               }]]);

  [mediator_ createPasskey];

  // Verify reauth was attempted.
  [(OCMockObject*)mock_reauth_module_ verify];
  // Client should not have fetched keys yet.
  EXPECT_FALSE(fake_client_->DidFetchKeys());

  // Trigger success.
  completionHandler(ReauthenticationResult::kSuccess);

  // Now client should have fetched keys.
  EXPECT_TRUE(fake_client_->DidFetchKeys());
}

// Tests that createPasskey handles reauth failure correctly.
TEST_F(PasskeyCreationBottomSheetMediatorTest, CreatePasskeyReauthFailure) {
  webauthn::PasskeyTabHelper* helper =
      webauthn::PasskeyTabHelper::FromWebState(web_state_);

  webauthn::RegistrationRequestParams params =
      webauthn::BuildRegistrationRequestParams({});

  mediator_ = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:&web_state_list_
                 requestID:params.RequestId()
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
                  canReusePreviousAuth:YES
                               handler:[OCMArg checkWithBlock:^BOOL(id obj) {
                                 completionHandler = [obj copy];
                                 return YES;
                               }]]);

  [mediator_ createPasskey];

  // Verify reauth was attempted.
  [(OCMockObject*)mock_reauth_module_ verify];
  EXPECT_FALSE(fake_client_->DidFetchKeys());

  // Expect dismissal on failure (as per current implementation logic in
  // mediator).
  [[mock_delegate_ expect] dismissPasskeyCreation];

  // Trigger failure.
  completionHandler(ReauthenticationResult::kFailure);

  // Verify dismissal.
  [(OCMockObject*)mock_delegate_ verify];
  // Verify client did NOT fetch keys.
  EXPECT_FALSE(fake_client_->DidFetchKeys());
}
