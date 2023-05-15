// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/authenticator.h"

#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_types.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/fake_keychain.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace device {

namespace {

using GetInfoCallback =
    test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>,
                               FidoRequestHandlerBase::RecognizedCredential>;
using GetAssertionCallback = test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    std::vector<AuthenticatorGetAssertionResponse>>;

constexpr char kRp1[] = "one.com";
constexpr char kRp2[] = "two.com";
const std::vector<uint8_t> kUserId1{1, 2, 3, 4};
const std::vector<uint8_t> kUserId2{5, 6, 7, 8};

class TouchIdAuthenticatorTest : public testing::Test,
                                 public base::test::WithFeatureOverride {
 protected:
  TouchIdAuthenticatorTest()
      : base::test::WithFeatureOverride(
            kWebAuthnMacPlatformAuthenticatorOptionalUv) {}

  base::test::SingleThreadTaskEnvironment task_environment_;
  fido::mac::AuthenticatorConfig config_{
      .keychain_access_group = "test-keychain-access-group",
      .metadata_secret = "TestMetadataSecret"};
  fido::mac::ScopedFakeKeychain keychain_{config_.keychain_access_group};
  fido::mac::TouchIdCredentialStore store_{config_};

  std::unique_ptr<fido::mac::TouchIdAuthenticator> authenticator_ =
      fido::mac::TouchIdAuthenticator::Create(config_);
};

TEST_P(TouchIdAuthenticatorTest, GetPlatformCredentialInfoForRequest_RK) {
  // Inject a resident credential for RP 1.
  PublicKeyCredentialUserEntity user(kUserId1);
  fido::mac::Credential credential =
      store_
          .CreateCredential(kRp1, user,
                            fido::mac::TouchIdCredentialStore::kDiscoverable)
          ->first;
  DiscoverableCredentialMetadata credential_metadata(
      AuthenticatorType::kTouchID, kRp1, credential.credential_id,
      std::move(user));

  // Inject a non resident credential for RP 2. This one should be ignored.
  PublicKeyCredentialUserEntity user2(kUserId2);
  ASSERT_TRUE(store_.CreateCredential(
      kRp2, std::move(user2),
      fido::mac::TouchIdCredentialStore::kNonDiscoverable));

  {
    // RP 1 should return the resident credential.
    GetInfoCallback callback;
    CtapGetAssertionRequest request(kRp1, "{json: true}");
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    EXPECT_THAT(std::get<0>(*callback.result()),
                testing::ElementsAre(credential_metadata));
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
  }
  {
    // RP 2 should return no credentials.
    GetInfoCallback callback;
    CtapGetAssertionRequest request(kRp2, "{json: true}");
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    EXPECT_TRUE(std::get<0>(*callback.result()).empty());
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
  }
}

TEST_P(TouchIdAuthenticatorTest, GetPlatformCredentialInfoForRequest_NonRK) {
  // Inject a non resident credential for RP 1.
  PublicKeyCredentialUserEntity user(kUserId1);
  fido::mac::Credential credential =
      store_
          .CreateCredential(kRp1, user,
                            fido::mac::TouchIdCredentialStore::kNonDiscoverable)
          ->first;
  DiscoverableCredentialMetadata credential_metadata(
      AuthenticatorType::kTouchID, kRp1, credential.credential_id,
      std::move(user));

  {
    // RP 1 should report the credential if it is in the allow list but not
    // return it.
    GetInfoCallback callback;
    CtapGetAssertionRequest request(kRp1, "{json: true}");
    request.allow_list.emplace_back(CredentialType::kPublicKey,
                                    credential.credential_id);
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    if (IsParamFeatureEnabled()) {
      EXPECT_THAT(std::get<0>(*callback.result()),
                  testing::ElementsAre(credential_metadata));
    } else {
      EXPECT_TRUE(std::get<0>(*callback.result()).empty());
    }
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
  }
  {
    // RP 1 should not report the credential if it is not in the allow list.
    GetInfoCallback callback;
    CtapGetAssertionRequest request(kRp1, "{json: true}");
    request.allow_list.emplace_back(CredentialType::kPublicKey,
                                    std::vector<uint8_t>{5, 6, 7, 8});
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    EXPECT_TRUE(std::get<0>(*callback.result()).empty());
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
  }
  {
    // RP 2 should report no credentials.
    GetInfoCallback callback;
    CtapGetAssertionRequest request(kRp2, "{json: true}");
    request.allow_list.emplace_back(CredentialType::kPublicKey,
                                    credential.credential_id);
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    EXPECT_TRUE(std::get<0>(*callback.result()).empty());
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
  }
}

TEST_P(TouchIdAuthenticatorTest, GetAssertionEmpty) {
  GetAssertionCallback callback;
  CtapGetAssertionRequest request(kRp1, "{json: true}");
  authenticator_->GetAssertion(std::move(request), CtapGetAssertionOptions(),
                               callback.callback());
  callback.WaitForCallback();
  EXPECT_EQ(callback.status(), CtapDeviceResponseCode::kCtap2ErrNoCredentials);
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(TouchIdAuthenticatorTest);

}  // namespace

}  // namespace device
