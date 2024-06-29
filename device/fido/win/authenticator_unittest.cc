// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/authenticator.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using MakeCredentialFuture =
    base::test::TestFuture<MakeCredentialStatus,
                           std::optional<AuthenticatorMakeCredentialResponse>>;

using GetAssertionFuture =
    base::test::TestFuture<GetAssertionStatus,
                           std::vector<AuthenticatorGetAssertionResponse>>;

using GetCredentialFuture =
    base::test::TestFuture<std::vector<DiscoverableCredentialMetadata>,
                           FidoRequestHandlerBase::RecognizedCredential>;

using EnumeratePlatformCredentialsFuture =
    base::test::TestFuture<std::vector<DiscoverableCredentialMetadata>>;

const std::vector<uint8_t> kCredentialId = {1, 2, 3, 4};
const std::vector<uint8_t> kCredentialId2 = {9, 0, 1, 2};
constexpr char kRpId[] = "project-altdeus.example.com";
const std::vector<uint8_t> kUserId = {5, 6, 7, 8};
constexpr char kUserName[] = "unit-aarc-noa";
constexpr char kUserDisplayName[] = "Noa";
const std::vector<uint8_t> kLargeBlob = {'b', 'l', 'o', 'b'};
const std::vector<uint8_t> kUserId2 = {1, 1, 1, 1};
constexpr char kUserName2[] = "chloe";
constexpr char kUserDisplayName2[] = "Chloe";

class WinAuthenticatorTest : public testing::Test {
 public:
  void SetUp() override {
    fake_webauthn_api_ = std::make_unique<FakeWinWebAuthnApi>();
    fake_webauthn_api_->set_supports_silent_discovery(true);
    authenticator_ = std::make_unique<WinWebAuthnApiAuthenticator>(
        /*current_window=*/nullptr, fake_webauthn_api_.get());
  }

  void SetVersion(int version) {
    fake_webauthn_api_->set_version(version);
    // `WinWebAuthnApiAuthenticator` does not expect the webauthn.dll version to
    // change during its lifetime, thus needs to be recreated for each version
    // change.
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
  GetCredentialFuture future;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  DiscoverableCredentialMetadata expected = DiscoverableCredentialMetadata(
      AuthenticatorType::kWinNative, kRpId, kCredentialId, user);
  EXPECT_EQ(std::get<0>(future.Get()),
            std::vector<DiscoverableCredentialMetadata>{expected});
  EXPECT_EQ(
      std::get<1>(future.Get()),
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
  EXPECT_FALSE(fake_webauthn_api_->last_get_credentials_options()
                   ->bBrowserInPrivateMode);
}

// Tests a request with the off the record flag on passes the
// bBrowserInPrivateMode option to the Windows API.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_Incognito) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  CtapGetAssertionOptions options;
  options.is_off_the_record_context = true;
  GetCredentialFuture future;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), std::move(options), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(fake_webauthn_api_->last_get_credentials_options()
                  ->bBrowserInPrivateMode);
}

// Tests getting credential information for an empty allow-list request that
// does not have valid credentials on a Windows version that supports silent
// discovery.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_NoCredentials) {
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialFuture future;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(std::get<0>(future.Get()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(
      std::get<1>(future.Get()),
      FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
}

// Tests the authenticator handling of an unexpected error from the Windows API.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_UnknownError) {
  fake_webauthn_api_->set_hresult(ERROR_NOT_SUPPORTED);
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialFuture future;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(std::get<0>(future.Get()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(std::get<1>(future.Get()),
            FidoRequestHandlerBase::RecognizedCredential::kUnknown);
}

// Tests the authenticator handling of attempting to get credential information
// for a version of the Windows API that does not support silent discovery.
TEST_F(WinAuthenticatorTest, GetCredentialInformationForRequest_Unsupported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(false);

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  GetCredentialFuture future;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  DiscoverableCredentialMetadata expected = DiscoverableCredentialMetadata(
      AuthenticatorType::kWinNative, kRpId, kCredentialId, user);
  EXPECT_EQ(std::get<0>(future.Get()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(std::get<1>(future.Get()),
            FidoRequestHandlerBase::RecognizedCredential::kUnknown);
}

// Tests that for non empty allow-list requests with a matching discoverable
// credential, the authenticator returns a credential list with only matching
// credentials.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_NonEmptyAllowList_Found) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user1(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user1);

  PublicKeyCredentialUserEntity user2(kUserId2, kUserName2, kUserDisplayName2);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId2, rp,
                                                   std::move(user2));

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  request.allow_list.emplace_back(CredentialType::kPublicKey, kCredentialId);
  GetCredentialFuture future;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  DiscoverableCredentialMetadata expected = DiscoverableCredentialMetadata(
      AuthenticatorType::kWinNative, kRpId, kCredentialId, user1);
  EXPECT_THAT(std::get<0>(future.Get()), testing::ElementsAre(expected));
  EXPECT_EQ(
      std::get<1>(future.Get()),
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
}

// Tests that for non empty allow-list requests without a matching discoverable
// credential, the authenticator returns an empty credential list and reports no
// credential availability.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_NonEmptyAllowList_NotMatching) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp,
                                                   std::move(user));

  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  request.allow_list.emplace_back(CredentialType::kPublicKey, kCredentialId2);
  GetCredentialFuture future;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(std::get<0>(future.Get()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(
      std::get<1>(future.Get()),
      FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
}

// Tests that for non empty allow-list requests without an internal transport
// credential, the authenticator returns an empty credential list and reports no
// credential availability, even if silent discovery is not supported.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_NonEmptyAllowList_NoInternal) {
  fake_webauthn_api_->set_supports_silent_discovery(false);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");

  PublicKeyCredentialDescriptor credential(CredentialType::kPublicKey,
                                           kCredentialId2);
  credential.transports = {FidoTransportProtocol::kUsbHumanInterfaceDevice,
                           FidoTransportProtocol::kHybrid};
  request.allow_list.emplace_back(std::move(credential));

  GetCredentialFuture future;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(std::get<0>(future.Get()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(
      std::get<1>(future.Get()),
      FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
}

// Tests that for non empty allow-list requests with an internal transport
// credential, the authenticator returns an empty credential list reports
// unknown credential availability when silent discovery is not supported.
TEST_F(WinAuthenticatorTest,
       GetCredentialInformationForRequest_NonEmptyAllowList_Internal) {
  fake_webauthn_api_->set_supports_silent_discovery(false);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");

  PublicKeyCredentialDescriptor credential(CredentialType::kPublicKey,
                                           kCredentialId2);
  credential.transports = {FidoTransportProtocol::kInternal,
                           FidoTransportProtocol::kHybrid};
  request.allow_list.emplace_back(std::move(credential));

  GetCredentialFuture future;
  authenticator_->GetPlatformCredentialInfoForRequest(
      std::move(request), CtapGetAssertionOptions(), future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_EQ(std::get<0>(future.Get()),
            std::vector<DiscoverableCredentialMetadata>{});
  EXPECT_EQ(std::get<1>(future.Get()),
            FidoRequestHandlerBase::RecognizedCredential::kUnknown);
}

TEST_F(WinAuthenticatorTest, EnumeratePlatformCredentials_NotSupported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(false);

  base::test::TestFuture<std::vector<DiscoverableCredentialMetadata>> future;
  WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      fake_webauthn_api_.get(), future.GetCallback());

  while (!future.IsReady()) {
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_TRUE(future.Get().empty());
}

TEST_F(WinAuthenticatorTest, EnumeratePlatformCredentials_Supported) {
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, rp, user);
  fake_webauthn_api_->set_supports_silent_discovery(true);

  base::test::TestFuture<std::vector<DiscoverableCredentialMetadata>> future;
  WinWebAuthnApiAuthenticator::EnumeratePlatformCredentials(
      fake_webauthn_api_.get(), future.GetCallback());

  while (!future.IsReady()) {
    base::RunLoop().RunUntilIdle();
  }

  std::vector<DiscoverableCredentialMetadata> creds = future.Take();
  ASSERT_EQ(creds.size(), 1u);
  const DiscoverableCredentialMetadata& cred = creds[0];
  EXPECT_EQ(cred.source, AuthenticatorType::kWinNative);
  EXPECT_EQ(cred.rp_id, kRpId);
  EXPECT_EQ(cred.cred_id, kCredentialId);
  EXPECT_EQ(cred.user.name, kUserName);
  EXPECT_EQ(cred.user.display_name, kUserDisplayName);
}

TEST_F(WinAuthenticatorTest, IsConditionalMediationAvailable) {
  for (bool silent_discovery : {false, true}) {
    SCOPED_TRACE(silent_discovery);
    fake_webauthn_api_->set_supports_silent_discovery(silent_discovery);
    base::test::TestFuture<bool> future;
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

    SetVersion(test_case.availability ? WEBAUTHN_API_VERSION_3
                                      : WEBAUTHN_API_VERSION_2);
    EXPECT_EQ(authenticator_->Options().large_blob_type.has_value(),
              test_case.availability);
    PublicKeyCredentialRpEntity rp("adrestian-empire.com");
    PublicKeyCredentialUserEntity user(std::vector<uint8_t>{1, 2, 3, 4},
                                       "el@adrestian-empire.com", "Edelgard");
    CtapMakeCredentialRequest request(
        test_data::kClientDataJson, rp, user,
        PublicKeyCredentialParams({{CredentialType::kPublicKey, -257}}));
    MakeCredentialOptions options;
    options.large_blob_support = test_case.requirement;
    MakeCredentialFuture future;
    authenticator_->MakeCredential(std::move(request), options,
                                   future.GetCallback());
    EXPECT_TRUE(future.Wait());
    ASSERT_EQ(std::get<0>(future.Get()), MakeCredentialStatus::kSuccess);
    EXPECT_EQ(std::get<1>(future.Get())->large_blob_type.has_value(),
              test_case.result);
  }
}

// Tests that making a credential with attachment=undefined forces the
// attachment to cross-platform if large blob is required.
// This is because largeBlob=required is ignored by the Windows platform
// authenticator at the time of writing (Feb 2023).
TEST_F(WinAuthenticatorTest, MakeCredentialLargeBlobAttachmentUndefined) {
  SetVersion(WEBAUTHN_API_VERSION_3);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  CtapMakeCredentialRequest request(
      test_data::kClientDataJson, rp, user,
      PublicKeyCredentialParams({{CredentialType::kPublicKey, -257}}));
  request.authenticator_attachment = AuthenticatorAttachment::kAny;
  fake_webauthn_api_->set_preferred_attachment(
      WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM);
  MakeCredentialOptions options;
  options.large_blob_support = LargeBlobSupport::kRequired;
  MakeCredentialFuture future;
  authenticator_->MakeCredential(std::move(request), options,
                                 future.GetCallback());
  EXPECT_TRUE(future.Wait());
  ASSERT_EQ(std::get<0>(future.Get()), MakeCredentialStatus::kSuccess);
  EXPECT_TRUE(std::get<1>(future.Get())->large_blob_type.has_value());
  EXPECT_NE(std::get<1>(future.Get())->transport_used,
            FidoTransportProtocol::kInternal);
}

TEST_F(WinAuthenticatorTest, GetAssertionLargeBlobNotSupported) {
  SetVersion(WEBAUTHN_API_VERSION_2);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, std::move(rp),
                                                   std::move(user));
  {
    // Read large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    GetAssertionFuture future;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(std::get<0>(future.Get()), GetAssertionStatus::kSuccess);
    EXPECT_FALSE(std::get<1>(future.Get()).at(0).large_blob.has_value());
  }
  {
    // Write large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_write = kLargeBlob;
    GetAssertionFuture future;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(std::get<0>(future.Get()), GetAssertionStatus::kSuccess);
    EXPECT_FALSE(std::get<1>(future.Get()).at(0).large_blob_written);
  }
}

TEST_F(WinAuthenticatorTest, GetAssertionLargeBlobError) {
  SetVersion(WEBAUTHN_API_VERSION_3);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, std::move(rp),
                                                   std::move(user));
  fake_webauthn_api_->set_large_blob_result(
      WEBAUTHN_CRED_LARGE_BLOB_STATUS_NOT_SUPPORTED);
  {
    // Read large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    GetAssertionFuture future;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(std::get<0>(future.Get()), GetAssertionStatus::kSuccess);
    EXPECT_FALSE(std::get<1>(future.Get()).at(0).large_blob.has_value());
  }
  {
    // Write large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_write = kLargeBlob;
    GetAssertionFuture future;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(std::get<0>(future.Get()), GetAssertionStatus::kSuccess);
    EXPECT_FALSE(std::get<1>(future.Get()).at(0).large_blob_written);
  }
}

TEST_F(WinAuthenticatorTest, GetAssertionLargeBlobSuccess) {
  SetVersion(WEBAUTHN_API_VERSION_3);
  PublicKeyCredentialRpEntity rp(kRpId);
  PublicKeyCredentialUserEntity user(kUserId, kUserName, kUserDisplayName);
  fake_webauthn_api_->InjectDiscoverableCredential(kCredentialId, std::move(rp),
                                                   std::move(user));
  {
    // Read large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    GetAssertionFuture future;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(std::get<0>(future.Get()), GetAssertionStatus::kSuccess);
    EXPECT_FALSE(std::get<1>(future.Get()).at(0).large_blob.has_value());
    EXPECT_FALSE(std::get<1>(future.Get()).at(0).large_blob_written);
  }
  {
    // Write large blob.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_write = kLargeBlob;
    GetAssertionFuture future;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(std::get<0>(future.Get()), GetAssertionStatus::kSuccess);
    EXPECT_FALSE(std::get<1>(future.Get()).at(0).large_blob.has_value());
    EXPECT_TRUE(std::get<1>(future.Get()).at(0).large_blob_written);
  }
  {
    // Read the large blob that was just written.
    CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
    CtapGetAssertionOptions options;
    options.large_blob_read = true;
    GetAssertionFuture future;
    authenticator_->GetAssertion(std::move(request), std::move(options),
                                 future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(std::get<0>(future.Get()), GetAssertionStatus::kSuccess);
    EXPECT_TRUE(std::get<1>(future.Get()).at(0).large_blob.has_value());
    EXPECT_EQ(*std::get<1>(future.Get()).at(0).large_blob, kLargeBlob);
    EXPECT_FALSE(std::get<1>(future.Get()).at(0).large_blob_written);
  }
}

}  // namespace
}  // namespace device
