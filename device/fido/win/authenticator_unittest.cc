// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/authenticator.h"

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using MakeCredentialCallbackReceiver = test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    absl::optional<AuthenticatorMakeCredentialResponse>>;

using GetCredentialCallbackReceiver =
    test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>,
                               bool>;

using EnumeratePlatformCredentialsCallbackReceiver =
    test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>>;

const std::vector<uint8_t> kCredentialId = {1, 2, 3, 4};
constexpr char kRpId[] = "project-altdeus.example.com";
const std::vector<uint8_t> kUserId = {5, 6, 7, 8};
constexpr char kUserName[] = "unit-aarc-noa";
constexpr char kUserDisplayName[] = "Noa";

class WinAuthenticatorTest : public testing::Test {
 public:
  void SetUp() override {
    fake_webauthn_api_ = std::make_unique<FakeWinWebAuthnApi>();
    fake_webauthn_api_->set_supports_silent_discovery(true);
    authenticator_ = std::make_unique<WinWebAuthnApiAuthenticator>(
        /*current_window=*/nullptr, fake_webauthn_api_.get());
  }

 protected:
  std::unique_ptr<FidoAuthenticator> authenticator_;
  std::unique_ptr<FakeWinWebAuthnApi> fake_webauthn_api_;
  base::test::TaskEnvironment task_environment;
};

// Tests getting credential information for an empty allow-list request that has
// valid credentials on a Windows version that supports silent discovery.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_HasCredentials) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  DiscoverableCredentialMetadata expected =
      DiscoverableCredentialMetadata(kRpId, kCredentialId, user);
  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{expected});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

// Tests getting credential information for an empty allow-list request that
// does not have valid credentials on a Windows version that supports silent
// discovery.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_NoCredentials) {
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

// Tests the authenticator handling of an unexpected error from the Windows API.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_UnknownError) {
  fake_webauthn_api_->set_hresult(ERROR_NOT_SUPPORTED);
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

// Tests the authenticator handling of attempting to get credential information
// for a version of the Windows API that does not support silent discovery.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_Unsupported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(false);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  DiscoverableCredentialMetadata expected =
      DiscoverableCredentialMetadata(kRpId, kCredentialId, user);
  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

// Tests that for non empty allow-list requests, the authenticator returns an
// empty credential list.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_NonEmptyAllowList) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  request.allow_list.emplace_back(CredentialType::kPublicKey, kCredentialId);
  GetCredentialCallbackReceiver callback;
  authenticator_->GetCredentialInformationForRequest(request,
                                                     callback.callback());
  callback.WaitForCallback();

  DiscoverableCredentialMetadata expected =
      DiscoverableCredentialMetadata(kRpId, kCredentialId, user);
  EXPECT_EQ(std::get<0>(*callback.result()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_TRUE(std::get<1>(*callback.result()));
}

TEST_F(WinAuthenticatorTest, EnumeratePlatformCredentials_NotSupported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(false);

  test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>>
      callback;
  WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      fake_webauthn_api_.get(), callback.callback());

  while (!callback.was_called()) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(std::get<0>(*callback.result()).empty());
}

TEST_F(WinAuthenticatorTest, EnumeratePlatformCredentials_Supported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(true);

  test::TestCallbackReceiver<std::vector<DiscoverableCredentialMetadata>>
      callback;
  WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      fake_webauthn_api_.get(), callback.callback());

  while (!callback.was_called()) {
    base::RunLoop().RunUntilIdle();
  }

  std::vector<DiscoverableCredentialMetadata> creds =
      std::move(std::get<0>(callback.TakeResult()));
  ASSERT_EQ(creds.size(), 1u);
  const DiscoverableCredentialMetadata& cred = creds[0];
  EXPECT_EQ(cred.rp_id, kRpId);
  EXPECT_EQ(cred.cred_id, kCredentialId);
  EXPECT_EQ(cred.user.name, kUserName);
  EXPECT_EQ(cred.user.display_name, kUserDisplayName);
}

TEST_F(WinAuthenticatorTest, IsConditionalMediationAvailable) {
  for (bool silent_discovery : {false, true}) {
    SCOPED_TRACE(silent_discovery);
    fake_webauthn_api_->set_supports_silent_discovery(silent_discovery);
    test::TestCallbackReceiver<bool> callback;
    base::RunLoop run_loop;
    WinWebAuthnApiAuthenticator::IsConditionalMediationAvailable(
        fake_webauthn_api_.get(),
        base::BindLambdaForTesting([&](bool is_available) {
          EXPECT_EQ(is_available, silent_discovery);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(WinAuthenticatorTest, MakeCredentialLargeBlob) {
  enum Availability : bool {
    kNotAvailable = false,
    kAvailable = true,
  };

  enum Result : bool {
    kDoesNotHaveLargeBlob = false,
    kHasLargeBlob = true,
  };

  struct LargeBlobTestCase {
    LargeBlobSupport requirement;
    Availability availability;
    Result result;
  };

  std::array<LargeBlobTestCase, 5> test_cases = {{
      {LargeBlobSupport::kNotRequested, kAvailable, kDoesNotHaveLargeBlob},
      {LargeBlobSupport::kNotRequested, kNotAvailable, kDoesNotHaveLargeBlob},
      {LargeBlobSupport::kPreferred, kAvailable, kHasLargeBlob},
      {LargeBlobSupport::kPreferred, kNotAvailable, kDoesNotHaveLargeBlob},
      {LargeBlobSupport::kRequired, kAvailable, kHasLargeBlob},
      // Calling the Windows API with large blob = required is not allowed if
      // it's not supported by the API version.
  }};

  for (const LargeBlobTestCase& test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "requirement=" << static_cast<bool>(test_case.requirement)
                 << ", availability="
                 << static_cast<bool>(test_case.availability));

    fake_webauthn_api_->set_supports_large_blobs(test_case.availability);
    EXPECT_EQ(authenticator_->SupportsLargeBlobs(), test_case.availability);
    PublicKeyCredentialRpEntity rp("adrestian-empire.com");
    PublicKeyCredentialUserEntity user(std::vector<uint8_t>{1, 2, 3, 4},
                                       "el@adrestian-empire.com", "Edelgard");
    CtapMakeCredentialRequest request(
        test_data::kClientDataJson, rp, user,
        PublicKeyCredentialParams({{CredentialType::kPublicKey, -257}}));
    MakeCredentialOptions options;
    options.large_blob_support = test_case.requirement;
    MakeCredentialCallbackReceiver callback;
    authenticator_->MakeCredential(std::move(request), options,
                                   callback.callback());
    callback.WaitForCallback();
    ASSERT_EQ(callback.status(), CtapDeviceResponseCode::kSuccess);
    EXPECT_EQ(callback.value()->has_associated_large_blob_key,
              test_case.result);
  }
}

}  // namespace
}  // namespace device
