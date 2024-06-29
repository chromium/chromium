// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/authenticator.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_types.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using GetInfoFuture =
    base::test::TestFuture<std::vector<DiscoverableCredentialMetadata>,
                           FidoRequestHandlerBase::RecognizedCredential>;
using GetAssertionFuture =
    base::test::TestFuture<GetAssertionStatus,
                           std::vector<AuthenticatorGetAssertionResponse>>;

constexpr char kRp1[] = "one.com";
constexpr char kRp2[] = "two.com";
const std::vector<uint8_t> kUserId1{1, 2, 3, 4};
const std::vector<uint8_t> kUserId2{5, 6, 7, 8};

class TouchIdAuthenticatorTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  fido::mac::AuthenticatorConfig config_{
      .keychain_access_group = "test-keychain-access-group",
      .metadata_secret = "TestMetadataSecret"};
  crypto::ScopedFakeAppleKeychainV2 keychain_{config_.keychain_access_group};
  fido::mac::TouchIdCredentialStore store_{config_};

  std::unique_ptr<fido::mac::TouchIdAuthenticator> authenticator_ =
      fido::mac::TouchIdAuthenticator::Create(config_);
};

TEST_F(TouchIdAuthenticatorTest, GetPlatformCredentialInfoForRequest_RK) {
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
    GetInfoFuture future;
    CtapGetAssertionRequest request(kRp1, "{json: true}");
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_THAT(std::get<0>(future.Get()),
                testing::ElementsAre(credential_metadata));
    EXPECT_EQ(
        std::get<1>(future.Get()),
        FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
  }
  {
    // RP 2 should return no credentials.
    GetInfoFuture future;
    CtapGetAssertionRequest request(kRp2, "{json: true}");
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_TRUE(std::get<0>(future.Get()).empty());
    EXPECT_EQ(
        std::get<1>(future.Get()),
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
  }
}

TEST_F(TouchIdAuthenticatorTest, GetPlatformCredentialInfoForRequest_NonRK) {
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
    GetInfoFuture future;
    CtapGetAssertionRequest request(kRp1, "{json: true}");
    request.allow_list.emplace_back(CredentialType::kPublicKey,
                                    credential.credential_id);
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_THAT(std::get<0>(future.Get()),
                testing::ElementsAre(credential_metadata));
    EXPECT_EQ(
        std::get<1>(future.Get()),
        FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
  }
  {
    // RP 1 should not report the credential if it is not in the allow list.
    GetInfoFuture future;
    CtapGetAssertionRequest request(kRp1, "{json: true}");
    request.allow_list.emplace_back(CredentialType::kPublicKey,
                                    std::vector<uint8_t>{5, 6, 7, 8});
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_TRUE(std::get<0>(future.Get()).empty());
    EXPECT_EQ(
        std::get<1>(future.Get()),
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
  }
  {
    // RP 2 should report no credentials.
    GetInfoFuture future;
    CtapGetAssertionRequest request(kRp2, "{json: true}");
    request.allow_list.emplace_back(CredentialType::kPublicKey,
                                    credential.credential_id);
    authenticator_->GetPlatformCredentialInfoForRequest(
        std::move(request), CtapGetAssertionOptions(), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_TRUE(std::get<0>(future.Get()).empty());
    EXPECT_EQ(
        std::get<1>(future.Get()),
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
  }
}

TEST_F(TouchIdAuthenticatorTest, GetAssertionEmpty) {
  GetAssertionFuture future;
  CtapGetAssertionRequest request(kRp1, "{json: true}");
  authenticator_->GetAssertion(std::move(request), CtapGetAssertionOptions(),
                               future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(std::get<0>(future.Get()),
            GetAssertionStatus::kUserConsentButCredentialNotRecognized);
}

}  // namespace

}  // namespace device
