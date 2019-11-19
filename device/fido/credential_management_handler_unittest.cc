// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/credential_management_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "device/fido/credential_management.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

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
        /*connector=*/nullptr, &virtual_device_factory_,
        base::flat_set<FidoTransportProtocol>{
            FidoTransportProtocol::kUsbHumanInterfaceDevice},
        ready_callback_.callback(),
        base::BindRepeating(&CredentialManagementHandlerTest::GetPIN,
                            base::Unretained(this)),
        finished_callback_.callback());
    return handler;
  }

  void GetPIN(int64_t num_attempts,
              base::OnceCallback<void(std::string)> provide_pin) {
    std::move(provide_pin).Run(kPIN);
  }

  base::test::TaskEnvironment task_environment_;

  test::TestCallbackReceiver<> ready_callback_;
  test::StatusAndValuesCallbackReceiver<
      CtapDeviceResponseCode,
      base::Optional<std::vector<AggregatedEnumerateCredentialsResponse>>,
      base::Optional<size_t>>
      get_credentials_callback_;
  test::ValueCallbackReceiver<CtapDeviceResponseCode> delete_callback_;
  test::ValueCallbackReceiver<CredentialManagementStatus> finished_callback_;
  test::VirtualFidoDeviceFactory virtual_device_factory_;
};

TEST_F(CredentialManagementHandlerTest, Test) {
  VirtualCtap2Device::Config ctap_config;
  ctap_config.pin_support = true;
  ctap_config.resident_key_support = true;
  ctap_config.credential_management_support = true;
  ctap_config.resident_credential_storage = 100;
  virtual_device_factory_.SetCtap2Config(ctap_config);
  virtual_device_factory_.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory_.mutable_state()->pin = kPIN;
  virtual_device_factory_.mutable_state()->retries = 8;

  PublicKeyCredentialRpEntity rp(kRPID, kRPName,
                                 /*icon_url=*/base::nullopt);
  PublicKeyCredentialUserEntity user(fido_parsing_utils::Materialize(kUserID),
                                     kUserName, kUserDisplayName,
                                     /*icon_url=*/base::nullopt);

  ASSERT_TRUE(virtual_device_factory_.mutable_state()->InjectResidentKey(
      kCredentialID, rp, user));

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  handler->GetCredentials(get_credentials_callback_.callback());
  get_credentials_callback_.WaitForCallback();

  auto result = get_credentials_callback_.TakeResult();
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

  handler->DeleteCredential(
      opt_response->front().credentials.front().credential_id,
      delete_callback_.callback());

  delete_callback_.WaitForCallback();
  ASSERT_EQ(CtapDeviceResponseCode::kSuccess, delete_callback_.value());
  EXPECT_EQ(virtual_device_factory_.mutable_state()->registrations.size(), 0u);
  EXPECT_FALSE(finished_callback_.was_called());
}

TEST_F(CredentialManagementHandlerTest,
       EnmerateCredentialResponse_TruncatedUTF8) {
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
  virtual_device_factory_.mutable_state()->retries = 8;

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
                                  base::StrCat({rp_name, kTruncatedUTF8}),
                                  /*icon_url=*/base::nullopt),
      PublicKeyCredentialUserEntity(
          fido_parsing_utils::Materialize(kUserID),
          base::StrCat({user_name, kTruncatedUTF8}),
          base::StrCat({display_name, kTruncatedUTF8}),
          /*icon_url=*/base::nullopt)));

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();
  handler->GetCredentials(get_credentials_callback_.callback());
  get_credentials_callback_.WaitForCallback();

  auto result = get_credentials_callback_.TakeResult();
  ASSERT_EQ(std::get<0>(result), CtapDeviceResponseCode::kSuccess);
  auto opt_response = std::move(std::get<1>(result));
  ASSERT_TRUE(opt_response);
  ASSERT_EQ(opt_response->size(), 1u);
  ASSERT_EQ(opt_response->front().credentials.size(), 1u);
  EXPECT_EQ(opt_response->front().rp,
            PublicKeyCredentialRpEntity(kRPID, rp_name,
                                        /*icon_url=*/base::nullopt));
  EXPECT_EQ(
      opt_response->front().credentials.front().user,
      PublicKeyCredentialUserEntity(fido_parsing_utils::Materialize(kUserID),
                                    user_name, display_name,
                                    /*icon_url=*/base::nullopt));
}

}  // namespace
}  // namespace device
