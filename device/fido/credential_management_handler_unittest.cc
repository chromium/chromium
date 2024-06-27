// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/credential_management_handler.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/fido/credential_management.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/large_blob.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using testing::UnorderedElementsAreArray;

constexpr char kPIN[] = "1234";
constexpr uint8_t kCredentialID[] = {0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
                                     0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa};
constexpr char kRPID[] = "example.com";
constexpr char kRPName[] = "Example Corp";
constexpr uint8_t kUserID[] = {0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1,
                               0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1};
constexpr char kUserName[] = "alice@example.com";
constexpr char kUserDisplayName[] = "Alice Example <alice@example.com>";

class CredentialManagementHandlerTest : public ::testing::Test {
 protected:
  std::unique_ptr<CredentialManagementHandler> MakeHandler() {
    auto handler = std::make_unique<CredentialManagementHandler>(
        &virtual_device_factory_,
        base::flat_set<FidoTransportProtocol>{
            FidoTransportProtocol::kUsbHumanInterfaceDevice},
        ready_future_.GetCallback(),
        base::BindRepeating(&CredentialManagementHandlerTest::GetPIN,
                            base::Unretained(this)),
        finished_future_.GetCallback());
    return handler;
  }

  void GetPIN(CredentialManagementHandler::AuthenticatorProperties
                  authenticator_properties,
              base::OnceCallback<void(std::string)> provide_pin) {
    std::move(provide_pin).Run(kPIN);
  }

  base::test::TaskEnvironment task_environment_;

  base::test::TestFuture<void> ready_future_;
  base::test::TestFuture<
      CtapDeviceResponseCode,
      std::optional<std::vector<AggregatedEnumerateCredentialsResponse>>,
      std::optional<size_t>>
      get_credentials_future_;
  base::test::TestFuture<CtapDeviceResponseCode> delete_future_;
  base::test::TestFuture<CtapDeviceResponseCode> update_user_info_future_;
  base::test::TestFuture<CredentialManagementStatus> finished_future_;
  test::VirtualFidoDeviceFactory virtual_device_factory_;
};

TEST_F(CredentialManagementHandlerTest, TestDeleteCredentials) {
  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.resident_credential_storage = 100;
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->pin_retries = device::kMaxPinRetries;

  PublicKeyCredentialRpEntity rp(kRPID, kRPName);
  PublicKeyCredentialUserEntity user(fido_parsing_utils::Materialize(kUserID),
                                     kUserName, kUserDisplayName);

  ASSERT_TRUE(virtual_device_factory_.mutable_state()->InjectResidentKey(
      kCredentialID, rp, user));

  auto handler = MakeHandler();
  ASSERT_TRUE(ready_future_.Wait());

  handler->GetCredentials(get_credentials_future_.GetCallback());
  EXPECT_TRUE(get_credentials_future_.Wait());

  auto result = get_credentials_future_.Take();
  ASSERT_EQ(std::get<0>(result), CtapDeviceResponseCode::kSuccess);
  auto opt_response = std::move(std::get<1>(result));
  ASSERT_TRUE(opt_response);
  EXPECT_EQ(opt_response->size(), 1u);
  EXPECT_EQ(opt_response->front().rp, rp);
  ASSERT_EQ(opt_response->front().credentials.size(), 1u);
  EXPECT_EQ(opt_response->front().credentials.front().user, user);

  auto num_remaining = std::get<2>(result);
  ASSERT_TRUE(num_remaining);
  EXPECT_EQ(*num_remaining, 99u);

  handler->DeleteCredentials(
      {opt_response->front().credentials.front().credential_id},
      delete_future_.GetCallback());

  EXPECT_TRUE(delete_future_.Wait());
  ASSERT_EQ(CtapDeviceResponseCode::kSuccess, delete_future_.Get());
  EXPECT_EQ(virtual_device_factory_.mutable_state()->registrations.size(), 0u);
  EXPECT_FALSE(finished_future_.IsReady());
}

// Tests that the credential management handler performs garbage collection when
// starting up.
TEST_F(CredentialManagementHandlerTest, TestGarbageCollectLargeBlob_Startup) {
  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.large_blob_support = true;
  ctap_config.pin_uv_auth_token_support = true;
  ctap_config.ctap2_versions = {Ctap2Version::kCtap2_1};
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->pin_retries = device::kMaxPinRetries;
  std::vector<uint8_t> empty_large_blob =
      virtual_device_factory_.mutable_state()->large_blob;

  PublicKeyCredentialRpEntity rp(kRPID, kRPName);
  PublicKeyCredentialUserEntity user(fido_parsing_utils::Materialize(kUserID),
                                     kUserName, kUserDisplayName);
  ASSERT_TRUE(virtual_device_factory_.mutable_state()->InjectResidentKey(
      kCredentialID, rp, user));

  std::vector<uint8_t> credential_id =
      fido_parsing_utils::Materialize(kCredentialID);
  LargeBlob blob(std::vector<uint8_t>{'b', 'l', 'o', 'b'}, 4);
  virtual_device_factory_.mutable_state()->InjectLargeBlob(
      &virtual_device_factory_.mutable_state()->registrations.at(credential_id),
      std::move(blob));
  ASSERT_NE(virtual_device_factory_.mutable_state()->large_blob,
            empty_large_blob);

  // Orphan the large blob by removing the credential.
  virtual_device_factory_.mutable_state()->registrations.clear();

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());
  EXPECT_EQ(virtual_device_factory_.mutable_state()->large_blob,
            empty_large_blob);
}

// Tests that CredentialManagementHandler::DeleteCredentials performs large blob
// garbage collection.
TEST_F(CredentialManagementHandlerTest, TestGarbageCollectLargeBlob_Delete) {
  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.large_blob_support = true;
  ctap_config.pin_uv_auth_token_support = true;
  ctap_config.ctap2_versions = {Ctap2Version::kCtap2_1};
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->pin_retries = device::kMaxPinRetries;
  std::vector<uint8_t> empty_large_blob =
      virtual_device_factory_.mutable_state()->large_blob;

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());

  PublicKeyCredentialRpEntity rp(kRPID, kRPName);
  PublicKeyCredentialUserEntity user(fido_parsing_utils::Materialize(kUserID),
                                     kUserName, kUserDisplayName);
  ASSERT_TRUE(virtual_device_factory_.mutable_state()->InjectResidentKey(
      kCredentialID, rp, user));

  std::vector<uint8_t> credential_id =
      fido_parsing_utils::Materialize(kCredentialID);
  LargeBlob blob(std::vector<uint8_t>{'b', 'l', 'o', 'b'}, 4);
  virtual_device_factory_.mutable_state()->InjectLargeBlob(
      &virtual_device_factory_.mutable_state()->registrations.at(credential_id),
      std::move(blob));
  ASSERT_NE(virtual_device_factory_.mutable_state()->large_blob,
            empty_large_blob);

  PublicKeyCredentialDescriptor credential(CredentialType::kPublicKey,
                                           credential_id);
  handler->DeleteCredentials({credential}, delete_future_.GetCallback());
  EXPECT_TRUE(delete_future_.Wait());
  ASSERT_EQ(CtapDeviceResponseCode::kSuccess, delete_future_.Get());
  EXPECT_EQ(virtual_device_factory_.mutable_state()->registrations.size(), 0u);
  EXPECT_EQ(virtual_device_factory_.mutable_state()->large_blob,
            empty_large_blob);
}

// Tests that CredentialManagementHandler::DeleteCredentials does not attempt
// large blob garbage collection if there is an error deleting the credential.
TEST_F(CredentialManagementHandlerTest,
       TestGarbageCollectLargeBlob_DeleteError) {
  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.large_blob_support = true;
  ctap_config.pin_uv_auth_token_support = true;
  ctap_config.ctap2_versions = {Ctap2Version::kCtap2_1};
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->pin_retries = device::kMaxPinRetries;
  std::vector<uint8_t> empty_large_blob =
      virtual_device_factory_.mutable_state()->large_blob;

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());

  PublicKeyCredentialRpEntity rp(kRPID, kRPName);
  PublicKeyCredentialUserEntity user(fido_parsing_utils::Materialize(kUserID),
                                     kUserName, kUserDisplayName);
  ASSERT_TRUE(virtual_device_factory_.mutable_state()->InjectResidentKey(
      kCredentialID, rp, user));
  std::vector<uint8_t> credential_id =
      fido_parsing_utils::Materialize(kCredentialID);
  LargeBlob blob(std::vector<uint8_t>{'b', 'l', 'o', 'b'}, 4);
  virtual_device_factory_.mutable_state()->InjectLargeBlob(
      &virtual_device_factory_.mutable_state()->registrations.at(credential_id),
      std::move(blob));
  ASSERT_NE(virtual_device_factory_.mutable_state()->large_blob,
            empty_large_blob);

  // Delete the credential directly from the authenticator.
  virtual_device_factory_.mutable_state()->registrations.clear();

  // Trying to delete the credential again should fail, and it should not
  // trigger garbage collection.
  PublicKeyCredentialDescriptor credential(CredentialType::kPublicKey,
                                           credential_id);
  handler->DeleteCredentials({credential}, delete_future_.GetCallback());
  EXPECT_TRUE(delete_future_.Wait());
  ASSERT_EQ(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
            delete_future_.Get());
  EXPECT_NE(virtual_device_factory_.mutable_state()->large_blob,
            empty_large_blob);
}

TEST_F(CredentialManagementHandlerTest, TestUpdateUserInformation) {
  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.resident_credential_storage = 100;
  ctap_config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->pin_retries = device::kMaxPinRetries;
  std::vector<uint8_t> credential_id =
      fido_parsing_utils::Materialize(kCredentialID);

  PublicKeyCredentialRpEntity rp(kRPID, kRPName);
  PublicKeyCredentialUserEntity user(fido_parsing_utils::Materialize(kUserID),
                                     kUserName, kUserDisplayName);

  ASSERT_TRUE(virtual_device_factory_.mutable_state()->InjectResidentKey(
      kCredentialID, rp, user));

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());

  PublicKeyCredentialUserEntity updated_user(
      fido_parsing_utils::Materialize(kUserID), "bobbyr@example.com",
      "Bobby R. Smith");

  handler->UpdateUserInformation(
      device::PublicKeyCredentialDescriptor(device::CredentialType::kPublicKey,
                                            credential_id),
      updated_user, update_user_info_future_.GetCallback());
  EXPECT_TRUE(update_user_info_future_.Wait());
  ASSERT_EQ(CtapDeviceResponseCode::kSuccess, update_user_info_future_.Get());

  EXPECT_EQ(virtual_device_factory_.mutable_state()
                ->registrations[credential_id]
                .user,
            updated_user);
  EXPECT_FALSE(finished_future_.IsReady());
}

TEST_F(CredentialManagementHandlerTest, TestForcePINChange) {
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->force_pin_change = true;

  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.min_pin_length_support = true;
  ctap_config.pin_uv_auth_token_support = true;
  ctap_config.ctap2_versions = {Ctap2Version::kCtap2_1};
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);

  auto handler = MakeHandler();
  EXPECT_TRUE(finished_future_.Wait());
  ASSERT_EQ(finished_future_.Get(),
            CredentialManagementStatus::kForcePINChange);
}

TEST_F(CredentialManagementHandlerTest,
       EnumerateCredentialResponse_TruncatedUTF8) {
  // Webauthn says[1] that authenticators may truncate strings in user entities.
  // Since authenticators aren't going to do UTF-8 processing, that means that
  // they may truncate a multi-byte code point and thus produce an invalid
  // string in the CBOR. This test exercises that case.
  //
  // [1] https://www.w3.org/TR/webauthn/#sctn-user-credential-params

  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.resident_credential_storage = 100;
  ctap_config.allow_invalid_utf8_in_credential_entities = true;
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->pin_retries = device::kMaxPinRetries;

  const std::string rp_name = base::StrCat({std::string(57, 'a'), "💣"});
  const std::string user_name = base::StrCat({std::string(57, 'b'), "💣"});
  const std::string display_name = base::StrCat({std::string(57, 'c'), "💣"});
  constexpr char kTruncatedUTF8[] = "\xf0\x9f\x92";

  // Simulate a truncated rp and user entity strings by appending a partial
  // UTF-8 sequence during InjectResidentKey(). The total string length
  // including the trailing sequence will be 64 bytes.
  DCHECK_EQ(rp_name.size(), 61u);

  ASSERT_TRUE(virtual_device_factory_.mutable_state()->InjectResidentKey(
      kCredentialID,
      PublicKeyCredentialRpEntity(kRPID,
                                  base::StrCat({rp_name, kTruncatedUTF8})),
      PublicKeyCredentialUserEntity(
          fido_parsing_utils::Materialize(kUserID),
          base::StrCat({user_name, kTruncatedUTF8}),
          base::StrCat({display_name, kTruncatedUTF8}))));

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());
  handler->GetCredentials(get_credentials_future_.GetCallback());
  EXPECT_TRUE(get_credentials_future_.Wait());

  auto result = get_credentials_future_.Take();
  ASSERT_EQ(std::get<0>(result), CtapDeviceResponseCode::kSuccess);
  auto opt_response = std::move(std::get<1>(result));
  ASSERT_TRUE(opt_response);
  ASSERT_EQ(opt_response->size(), 1u);
  ASSERT_EQ(opt_response->front().credentials.size(), 1u);
  EXPECT_EQ(opt_response->front().rp,
            PublicKeyCredentialRpEntity(kRPID, rp_name));
  EXPECT_EQ(
      opt_response->front().credentials.front().user,
      PublicKeyCredentialUserEntity(fido_parsing_utils::Materialize(kUserID),
                                    user_name, display_name));
}

TEST_F(CredentialManagementHandlerTest, EnumerateCredentialsMultipleRPs) {
  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.resident_credential_storage = 100;
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->pin_retries = device::kMaxPinRetries;

  const PublicKeyCredentialRpEntity rps[] = {
      {"foo.com", "foo"},
      {"bar.com", "bar"},
      {"foobar.com", "foobar"},
  };
  const PublicKeyCredentialUserEntity users[] = {
      {{0}, "alice", "Alice"},
      {{1}, "bob", "Bob"},
  };

  uint8_t credential_id[] = {0};
  for (const auto& rp : rps) {
    for (const auto& user : users) {
      ASSERT_TRUE(virtual_device_factory_.mutable_state()->InjectResidentKey(
          credential_id, rp, user));
      credential_id[0]++;
    }
  }

  auto handler = MakeHandler();
  EXPECT_TRUE(ready_future_.Wait());

  handler->GetCredentials(get_credentials_future_.GetCallback());
  EXPECT_TRUE(get_credentials_future_.Wait());

  auto result = get_credentials_future_.Take();
  ASSERT_EQ(std::get<0>(result), CtapDeviceResponseCode::kSuccess);

  std::vector<AggregatedEnumerateCredentialsResponse> responses =
      std::move(*std::get<1>(result));
  ASSERT_EQ(responses.size(), 3u);

  PublicKeyCredentialRpEntity got_rps[3];
  base::ranges::transform(responses, std::begin(got_rps),
                          &AggregatedEnumerateCredentialsResponse::rp);
  EXPECT_THAT(got_rps, UnorderedElementsAreArray(rps));

  for (const AggregatedEnumerateCredentialsResponse& response : responses) {
    ASSERT_EQ(response.credentials.size(), 2u);
    PublicKeyCredentialUserEntity got_users[2];
    std::transform(response.credentials.begin(), response.credentials.end(),
                   std::begin(got_users),
                   [](const auto& credential) { return credential.user; });
    EXPECT_THAT(got_users, UnorderedElementsAreArray(users));
  }
}

}  // namespace
}  // namespace device
