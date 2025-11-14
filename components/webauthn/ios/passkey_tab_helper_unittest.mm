// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_tab_helper.h"

#import "base/base64url.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/core/browser/test_passkey_model.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

constexpr char kCredentialId[] = "credential_id";
constexpr char kCredentialId2[] = "credential_id_2";
constexpr char kRpId[] = "example.com";

constexpr char kWebAuthenticationIOSContentAreaEventHistogram[] =
    "WebAuthentication.IOS.ContentAreaEvent";

// Converts an std::string to a uint8_t vector.
std::vector<uint8_t> AsByteVector(std::string str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

// Builds RegistrationRequestParams from an exclude credentials list.
PasskeyTabHelper::RegistrationRequestParams BuildRegistrationRequestParams(
    const std::vector<device::PublicKeyCredentialDescriptor>&
        exclude_credentials) {
  std::string frame_id = "";
  device::PublicKeyCredentialRpEntity rp_entity(kRpId);
  std::vector<uint8_t> challenge;
  device::PublicKeyCredentialUserEntity user_entity;
  return PasskeyTabHelper::RegistrationRequestParams(
      PasskeyTabHelper::RequestParams(
          frame_id, std::move(rp_entity), std::move(challenge),
          device::UserVerificationRequirement::kPreferred),
      std::move(user_entity), exclude_credentials);
}

}  // namespace

class FakeIOSPasskeyClient : public IOSPasskeyClient {
 public:
  FakeIOSPasskeyClient() = default;
  ~FakeIOSPasskeyClient() override = default;

  bool PerformUserVerification() override { return false; }
  void FetchKeys(webauthn::ReauthenticatePurpose purpose,
                 webauthn::KeysFetchedCallback callback) override {
    if (!callback.is_null()) {
      std::move(callback).Run({});
    }
  }
  void ShowSuggestionBottomSheet() override {}
  void AllowPasskeyCreationInfobar(bool allowed) override {}
};

class PasskeyTabHelperTest : public PlatformTest {
 public:
  PasskeyTabHelperTest() {
    PasskeyTabHelper::CreateForWebState(
        &fake_web_state_, passkey_model_.get(),
        std::make_unique<FakeIOSPasskeyClient>());
  }

 protected:
  PasskeyTabHelper* passkey_tab_helper() {
    return PasskeyTabHelper::FromWebState(&fake_web_state_);
  }

  bool HasExcludedPasskey(
      const PasskeyTabHelper::RegistrationRequestParams& params) {
    return passkey_tab_helper()->HasExcludedPasskey(params);
  }

  web::WebTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<webauthn::PasskeyModel> passkey_model_ =
      std::make_unique<webauthn::TestPasskeyModel>();
  web::FakeWebState fake_web_state_;
};

TEST_F(PasskeyTabHelperTest, LogsEventFromGetRequestedString) {
  passkey_tab_helper()->LogEventFromString("getRequested");

  constexpr int kGetRequestedBucket = 0;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kGetRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsEventFromCreateRequestedString) {
  passkey_tab_helper()->LogEventFromString("createRequested");

  constexpr int kCreateRequestedBucket = 1;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kCreateRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsGetResolvedEventGpmPasskey) {
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_credential_id(kCredentialId);
  passkey.set_rp_id(kRpId);
  passkey_model_->AddNewPasskeyForTesting(std::move(passkey));

  std::string credential_id_base64url_encoded;
  base::Base64UrlEncode(kCredentialId,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &credential_id_base64url_encoded);
  passkey_tab_helper()->HandleGetResolvedEvent(credential_id_base64url_encoded,
                                               kRpId);

  constexpr int kGetResolvedGpmBucket = 2;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kGetResolvedGpmBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsGetResolvedEventNonGpmPasskey) {
  std::string credential_id_base64url_encoded;
  base::Base64UrlEncode(kCredentialId,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &credential_id_base64url_encoded);
  passkey_tab_helper()->HandleGetResolvedEvent(credential_id_base64url_encoded,
                                               kRpId);

  constexpr int kGetResolvedNonGpmBucket = 3;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kGetResolvedNonGpmBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsEventFromCreateResolvedGpmString) {
  passkey_tab_helper()->LogEventFromString("createResolvedGpm");

  constexpr int kCreateRequestedBucket = 4;
  histogram_tester_.ExpectUniqueSample(
      kWebAuthenticationIOSContentAreaEventHistogram, kCreateRequestedBucket,
      /*count=*/1);
}

TEST_F(PasskeyTabHelperTest, LogsEventFromCreateResolvedNonGpmString) {
  passkey_tab_helper()->LogEventFromString("createResolvedNonGpm");

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
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_credential_id(kCredentialId);
  passkey.set_rp_id(kRpId);
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
