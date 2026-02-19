// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_tab_helper.h"

#import "base/rand_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "components/password_manager/core/browser/mock_password_manager.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "components/webauthn/ios/fake_ios_passkey_client.h"
#import "components/webauthn/ios/ios_webauthn_credentials_delegate.h"
#import "components/webauthn/ios/passkey_java_script_feature.h"
#import "components/webauthn/ios/passkey_test_util.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace webauthn {

using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::kCreateRequested;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
    kCreateResolvedGpm;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
    kCreateResolvedNonGpm;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::kGetRequested;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::kGetResolvedGpm;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
    kGetResolvedNonGpm;
using PasskeyTabHelper::WebAuthenticationIOSContentAreaEvent::
    kIncognitoInterstitialShown;

namespace {

constexpr char kCredentialId[] = "credential_id";
constexpr char kCredentialId2[] = "credential_id_2";
constexpr char kWellKnownURL[] = "https://example.com/.well-known/webauthn";
constexpr char kOriginURL[] = "https://example.com";
constexpr char kRelatedOriginURL[] = "https://example.ca";

constexpr char kWebAuthenticationIOSContentAreaEventHistogram[] =
    "WebAuthentication.IOS.ContentAreaEvent";
}  // namespace

class PasskeyTabHelperTest : public PlatformTest {
 public:
  PasskeyTabHelperTest()
      : scoped_web_client_(std::make_unique<web::FakeWebClient>()) {
    static_cast<web::FakeWebClient*>(scoped_web_client_.Get())
        ->SetJavaScriptFeatures({PasskeyJavaScriptFeature::GetInstance()});
    fake_web_state_.SetBrowserState(&fake_browser_state_);
    fake_browser_state_.SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    web::test::OverrideJavaScriptFeatures(
        &fake_browser_state_, {PasskeyJavaScriptFeature::GetInstance()});

    auto client = std::make_unique<FakeIOSPasskeyClient>(&fake_web_state_);
    client_ = client.get();
    PasskeyTabHelper::CreateForWebState(&fake_web_state_, passkey_model_.get(),
                                        std::move(client));
  }

 protected:
  PasskeyTabHelper* passkey_tab_helper() {
    return PasskeyTabHelper::FromWebState(&fake_web_state_);
  }

  bool HasExcludedPasskey(const RegistrationRequestParams& params) {
    return passkey_tab_helper()->HasExcludedPasskey(params);
  }

  // Returns the list of passkeys filtered by the allowed credentials list.
  std::vector<sync_pb::WebauthnCredentialSpecifics> GetFilteredPasskeys(
      const AssertionRequestParams& params) {
    return passkey_tab_helper()->GetFilteredPasskeys(params);
  }

  // Sets up a web frame manager with a web frame.
  void SetUpWebFramesManagerAndWebFrame(const GURL& origin) {
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    auto frame = web::FakeWebFrame::CreateMainWebFrame(origin);
    frame->set_browser_state(&fake_browser_state_);
    frames_manager->AddWebFrame(std::move(frame));
    fake_web_state_.SetWebFramesManager(
        PasskeyJavaScriptFeature::GetInstance()->GetSupportedContentWorld(),
        std::move(frames_manager));
  }

  // Sets up the IOSPasswordManagerDriver needed to retrieve the
  // WebAuthnCredentialsDelegate.
  void SetUpIOSPasswordManagerDriver() {
    IOSPasswordManagerDriverFactory::CreateForWebState(
        &fake_web_state_, OCMStrictClassMock([SharedPasswordController class]),
        &password_manager_);
  }

  void SetUpRelatedOrigin() {
    SetUpIOSPasswordManagerDriver();
    SetUpWebFramesManagerAndWebFrame(GURL(kRelatedOriginURL));
  }

  // Sets up mock response for .well-known/webauthn with provided parameters.
  void SetUpMockWellKnownResponse(const std::string& body,
                                  const std::string& content_type,
                                  const std::string& mime_type,
                                  net::Error net_error) {
    network::mojom::URLResponseHeadPtr head =
        network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    head->headers->SetHeader("Content-Type", content_type);
    head->mime_type = mime_type;
    test_url_loader_factory_.AddResponse(
        GURL(kWellKnownURL), std::move(head), body,
        network::URLLoaderCompletionStatus(net_error));
  }

  // Returns a random request ID of the same charset as the fake request ID.
  std::string GetUniqueRequestId() {
    return base::HexEncodeLower(base::RandBytesAsVector(16));
  }

  // Verifies that ShouldPerformUserVerification returns the expected results
  // with and without biometric authentication enabled.
  void VerifyShouldPerformUserVerification(const std::string& request_id,
                                           bool expected_with_biometrics,
                                           bool expected_without_biometrics) {
    SCOPED_TRACE(testing::Message() << "ID: " << request_id);
    EXPECT_EQ(passkey_tab_helper()->ShouldPerformUserVerification(
                  request_id, /*is_biometric_authentication_enabled=*/true),
              std::optional<bool>(expected_with_biometrics));
    EXPECT_EQ(passkey_tab_helper()->ShouldPerformUserVerification(
                  request_id, /*is_biometric_authentication_enabled=*/false),
              std::optional<bool>(expected_without_biometrics));
  }

  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient scoped_web_client_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<PasskeyModel> passkey_model_ =
      std::make_unique<TestPasskeyModel>();
  web::FakeBrowserState fake_browser_state_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  web::FakeWebState fake_web_state_;
  raw_ptr<FakeIOSPasskeyClient> client_ = nullptr;
  password_manager::MockPasswordManager password_manager_;
};

TEST_F(PasskeyTabHelperTest, LogsEventFromGetRequested) {
  passkey_tab_helper()->LogEvent(kGetRequested);

  constexpr int kGetRequestedBucket = 0;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kGetRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsEventFromCreateRequested) {
  passkey_tab_helper()->LogEvent(kCreateRequested);

  constexpr int kCreateRequestedBucket = 1;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kCreateRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsGetResolvedEventGpmPasskey) {
  passkey_tab_helper()->LogEvent(kGetResolvedGpm);

  constexpr int kGetResolvedGpmBucket = 2;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kGetResolvedGpmBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsGetResolvedEventNonGpmPasskey) {
  passkey_tab_helper()->LogEvent(kGetResolvedNonGpm);

  constexpr int kGetResolvedNonGpmBucket = 3;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kGetResolvedNonGpmBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsEventFromCreateResolvedGpm) {
  passkey_tab_helper()->LogEvent(kCreateResolvedGpm);

  constexpr int kCreateRequestedBucket = 4;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kCreateRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsEventFromCreateResolvedNonGpm) {
  passkey_tab_helper()->LogEvent(kCreateResolvedNonGpm);

  constexpr int kCreateRequestedBucket = 5;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kCreateRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, HasExcludedPasskey) {
  // Empty exclude credentials list, expecting no match.
  std::vector<device::PublicKeyCredentialDescriptor> exclude_credentials;
  ASSERT_FALSE(
      HasExcludedPasskey(BuildRegistrationRequestParams(exclude_credentials)));

  // Add passkey with kCredentialId.
  sync_pb::WebauthnCredentialSpecifics passkey = GetTestPasskey(kCredentialId);
  passkey_model_->AddNewPasskeyForTesting(std::move(passkey));

  // Add kCredentialId2 to exclude credentials list, expecting no match.
  exclude_credentials.push_back(
      {device::CredentialType::kPublicKey, AsByteVector(kCredentialId2)});
  ASSERT_FALSE(
      HasExcludedPasskey(BuildRegistrationRequestParams(exclude_credentials)));

  // Add kCredentialId to exclude credentials list, expecting a match.
  exclude_credentials.push_back(
      {device::CredentialType::kPublicKey, AsByteVector(kCredentialId)});
  ASSERT_TRUE(
      HasExcludedPasskey(BuildRegistrationRequestParams(exclude_credentials)));
}

TEST_F(PasskeyTabHelperTest, FilterPasskeys) {
  // Add passkey with kCredentialId.
  sync_pb::WebauthnCredentialSpecifics passkey = GetTestPasskey(kCredentialId);
  passkey_model_->AddNewPasskeyForTesting(std::move(passkey));

  // Add passkey with kCredentialId2.
  sync_pb::WebauthnCredentialSpecifics passkey2 =
      GetTestPasskey(kCredentialId2);
  passkey_model_->AddNewPasskeyForTesting(std::move(passkey2));

  // Make sure 2 distinct passkeys were added.
  std::vector<sync_pb::WebauthnCredentialSpecifics> passkeys =
      passkey_model_->GetPasskeys(kRpId,
                                  PasskeyModel::ShadowedCredentials::kExclude);
  EXPECT_EQ(passkeys.size(), 2u);

  // Empty allow credentials list, expect no filtering.
  std::vector<device::PublicKeyCredentialDescriptor> allow_credentials;
  std::vector<sync_pb::WebauthnCredentialSpecifics> filtered_passkeys =
      GetFilteredPasskeys(BuildAssertionRequestParams(allow_credentials));
  EXPECT_EQ(filtered_passkeys.size(), 2u);

  // Add kCredentialId2 to allow credentials list, expect 1 filtered passkey.
  allow_credentials.push_back(
      {device::CredentialType::kPublicKey, AsByteVector(kCredentialId2)});
  filtered_passkeys =
      GetFilteredPasskeys(BuildAssertionRequestParams(allow_credentials));
  EXPECT_EQ(filtered_passkeys.size(), 1u);

  // Add kCredentialId to allow credentials list, expect 1 filtered passkey.
  allow_credentials.clear();
  allow_credentials.push_back(
      {device::CredentialType::kPublicKey, AsByteVector(kCredentialId)});
  filtered_passkeys =
      GetFilteredPasskeys(BuildAssertionRequestParams(allow_credentials));
  EXPECT_EQ(filtered_passkeys.size(), 1u);

  // Have both credentials in the allow credentials list, expect no filtering.
  allow_credentials.push_back(
      {device::CredentialType::kPublicKey, AsByteVector(kCredentialId2)});
  filtered_passkeys =
      GetFilteredPasskeys(BuildAssertionRequestParams(allow_credentials));
  EXPECT_EQ(filtered_passkeys.size(), 2u);
}

// Tests that the IOSWebAuthnCredentialsDelegate receives the available passkeys
// when a passkey assertion request is processed.
TEST_F(PasskeyTabHelperTest, SendPasskeysToWebAuthnCredentialsDelegate) {
  SetUpWebFramesManagerAndWebFrame(GURL(kOriginURL));
  SetUpIOSPasswordManagerDriver();

  // Add passkey with `kCredentialId`.
  sync_pb::WebauthnCredentialSpecifics passkey = GetTestPasskey(kCredentialId);
  passkey_model_->AddNewPasskeyForTesting(std::move(passkey));

  // Verify that the delegate has no passkeys initially.
  auto passkeys_before = client_->delegate()->GetPasskeys();
  ASSERT_FALSE(passkeys_before.has_value());
  EXPECT_EQ(passkeys_before.error(),
            password_manager::WebAuthnCredentialsDelegate::
                PasskeysUnavailableReason::kNotReceived);

  AssertionRequestParams params = BuildAssertionRequestParams({});
  passkey_tab_helper()->HandleGetRequestedEvent(std::move(params));

  // Verify that the delegate has received the passkey.
  auto passkeys_after = client_->delegate()->GetPasskeys();
  ASSERT_TRUE(passkeys_after.has_value());
  EXPECT_EQ(passkeys_after.value()->size(), 1u);
  EXPECT_EQ(passkeys_after.value()->at(0).credential_id(),
            AsByteVector(kCredentialId));
}

// Tests that example.ca can access passkeys using relying party id
// example.com when remote validation passes.
TEST_F(PasskeyTabHelperTest, RequestPasskeyFromRelatedOriginSuccess) {
  SetUpRelatedOrigin();

  SetUpMockWellKnownResponse(R"({ "origins": ["https://example.ca"] })",
                             "application/json", "application/json", net::OK);

  passkey_tab_helper()->HandleGetRequestedEvent(
      BuildAssertionRequestParams({}));

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return client_->DidShowSuggestionBottomSheet(); }));
}

// Tests that example.ca can not access passkeys using relying party id
// example.com when remote validation fails.
TEST_F(PasskeyTabHelperTest, RequestPasskeyFromRelatedOriginFailure) {
  SetUpRelatedOrigin();

  SetUpMockWellKnownResponse(R"({ "origins": ["https://example.uk"] })",
                             "application/json", "application/json", net::OK);

  passkey_tab_helper()->HandleGetRequestedEvent(
      BuildAssertionRequestParams({}));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !passkey_tab_helper()->HasPendingValidationForTesting();
  }));

  EXPECT_FALSE(client_->DidShowSuggestionBottomSheet());
}

// Tests that example.ca can register a passkey using relying party id
// example.com when remote validation passes.
TEST_F(PasskeyTabHelperTest, CreatePasskeyFromRelatedOriginSuccess) {
  SetUpRelatedOrigin();

  SetUpMockWellKnownResponse(R"({ "origins": ["https://example.ca"] })",
                             "application/json", "application/json", net::OK);

  passkey_tab_helper()->HandleCreateRequestedEvent(
      BuildRegistrationRequestParams({}));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !passkey_tab_helper()->HasPendingValidationForTesting();
  }));

  EXPECT_TRUE(client_->DidShowCreationBottomSheet());
}

// Tests that example.ca can not register a passkey using relying party id
// example.com when remote validation fails.
TEST_F(PasskeyTabHelperTest, CreatePasskeyFromRelatedOriginFailure) {
  SetUpRelatedOrigin();

  SetUpMockWellKnownResponse(R"({ "origins": ["https://example.uk"] })",
                             "application/json", "application/json", net::OK);

  passkey_tab_helper()->HandleCreateRequestedEvent(
      BuildRegistrationRequestParams({}));

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !passkey_tab_helper()->HasPendingValidationForTesting();
  }));

  EXPECT_FALSE(client_->DidShowCreationBottomSheet());
}

// Tests that ShouldPerformUserVerification returns the correct value for
// assertion and registration requests.
TEST_F(PasskeyTabHelperTest, ShouldPerformUserVerification) {
  SetUpWebFramesManagerAndWebFrame(GURL(kOriginURL));
  SetUpIOSPasswordManagerDriver();

  // Test with non-existent request ID.
  EXPECT_EQ(
      passkey_tab_helper()->ShouldPerformUserVerification("non-existent", true),
      std::nullopt);

  // An array of user verification requirements, and their expected values.
  struct UserVerificationRequirementTest {
    device::UserVerificationRequirement requirement;
    bool expected_with_biometrics;
    bool expected_without_biometrics;
  };

  std::vector<UserVerificationRequirementTest> user_verification_requirements =
      {{device::UserVerificationRequirement::kPreferred,
        /*expected_with_biometrics=*/true,
        /*expected_without_biometrics=*/false},
       {device::UserVerificationRequirement::kRequired,
        /*expected_with_biometrics=*/true,
        /*expected_without_biometrics=*/true},
       {device::UserVerificationRequirement::kDiscouraged,
        /*expected_with_biometrics=*/false,
        /*expected_without_biometrics=*/false}};

  // Tests assertion requests.
  for (const auto& test : user_verification_requirements) {
    std::string request_id = GetUniqueRequestId();
    passkey_tab_helper()->HandleGetRequestedEvent(
        BuildAssertionRequestParams({}, test.requirement, request_id));
    VerifyShouldPerformUserVerification(request_id,
                                        test.expected_with_biometrics,
                                        test.expected_without_biometrics);
  }

  // Tests registration requests.
  for (const auto& test : user_verification_requirements) {
    std::string request_id = GetUniqueRequestId();
    passkey_tab_helper()->HandleCreateRequestedEvent(
        BuildRegistrationRequestParams({}, test.requirement, request_id));
    VerifyShouldPerformUserVerification(request_id,
                                        test.expected_with_biometrics,
                                        test.expected_without_biometrics);
  }
}

TEST_F(PasskeyTabHelperTest, ShowCreationInterstitialAndContinue) {
  fake_browser_state_.SetOffTheRecord(true);

  bool callback_executed = false;
  bool callback_result = false;
  auto callback = base::BindOnce(
      [](bool* executed, bool* result, bool proceed) {
        *executed = true;
        *result = proceed;
      },
      &callback_executed, &callback_result);

  client_->SetInterstitialProceeds(true);
  EXPECT_TRUE(passkey_tab_helper()->ShowCreationInterstitialIfNecessary(
      std::move(callback)));

  EXPECT_TRUE(client_->DidShowInterstitial());
  EXPECT_TRUE(callback_executed);
  EXPECT_TRUE(callback_result);
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram,
      static_cast<int>(kIncognitoInterstitialShown),
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, ShowCreationInterstitialAndCancel) {
  fake_browser_state_.SetOffTheRecord(true);

  bool callback_executed = false;
  bool callback_result = true;
  auto callback = base::BindOnce(
      [](bool* executed, bool* result, bool proceed) {
        *executed = true;
        *result = proceed;
      },
      &callback_executed, &callback_result);

  client_->SetInterstitialProceeds(false);
  EXPECT_TRUE(passkey_tab_helper()->ShowCreationInterstitialIfNecessary(
      std::move(callback)));

  EXPECT_TRUE(client_->DidShowInterstitial());
  EXPECT_TRUE(callback_executed);
  EXPECT_FALSE(callback_result);
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram,
      static_cast<int>(kIncognitoInterstitialShown),
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, NoCreationInterstitial) {
  fake_browser_state_.SetOffTheRecord(false);

  bool callback_executed = false;
  auto callback =
      base::BindOnce([](bool* executed, bool proceed) { *executed = true; },
                     &callback_executed);

  EXPECT_FALSE(passkey_tab_helper()->ShowCreationInterstitialIfNecessary(
      std::move(callback)));

  EXPECT_FALSE(client_->DidShowInterstitial());
  EXPECT_FALSE(callback_executed);
  histogram_tester_.ExpectTotalCount(
      kWebAuthenticationIOSContentAreaEventHistogram, 0);
}

}  // namespace webauthn
