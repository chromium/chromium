// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_fido_device_authenticator.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using CredentialInfoCallback = device::test::TestCallbackReceiver<
    std::vector<DiscoverableCredentialMetadata>,
    FidoRequestHandlerBase::RecognizedCredential>;

class VirtualFidoDeviceAuthenticatorTest : public testing::Test {
 protected:
  void SetUp() override {
    VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.resident_key_support = true;
    config.pin_uv_auth_token_support = true;
    config.ctap2_versions = {Ctap2Version::kCtap2_1};

    authenticator_state_ = base::MakeRefCounted<VirtualFidoDevice::State>();
    auto virtual_device =
        std::make_unique<VirtualCtap2Device>(authenticator_state_, config);
    virtual_device_ = virtual_device.get();
    authenticator_ = std::make_unique<VirtualFidoDeviceAuthenticator>(
        std::move(virtual_device));

    device::test::TestCallbackReceiver<> callback;
    authenticator_->InitializeAuthenticator(callback.callback());
    callback.WaitForCallback();
  }

 protected:
  scoped_refptr<VirtualFidoDevice::State> authenticator_state_;
  std::unique_ptr<VirtualFidoDeviceAuthenticator> authenticator_;
  raw_ptr<VirtualCtap2Device> virtual_device_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(VirtualFidoDeviceAuthenticatorTest,
       TestGetPlatformCredentialInfoForRequest) {
  constexpr char kRpId[] = "forgerfamily.com";
  CtapGetAssertionRequest request(kRpId, /*client_data_json=*/"");
  authenticator_state_->transport = FidoTransportProtocol::kInternal;
  {
    // No credentials.
    CredentialInfoCallback callback;
    authenticator_->GetPlatformCredentialInfoForRequest(
        request, CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(std::get<0>(*callback.result()),
              std::vector<DiscoverableCredentialMetadata>{});
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
  }
  {
    // A credential for a different RP ID.
    ASSERT_TRUE(authenticator_state_->InjectResidentKey(
        std::vector<uint8_t>{1, 2, 3, 4},
        PublicKeyCredentialRpEntity("eden-academy.com"),
        PublicKeyCredentialUserEntity()));
    CredentialInfoCallback callback;
    authenticator_->GetPlatformCredentialInfoForRequest(
        request, CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(std::get<0>(*callback.result()),
              std::vector<DiscoverableCredentialMetadata>{});
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
    authenticator_state_->registrations.clear();
  }
  {
    // A non-resident credential with an empty allow-list.
    ASSERT_TRUE(authenticator_state_->InjectRegistration(
        std::vector<uint8_t>{1, 2, 3, 4}, kRpId));
    CredentialInfoCallback callback;
    authenticator_->GetPlatformCredentialInfoForRequest(
        request, CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    EXPECT_EQ(std::vector<DiscoverableCredentialMetadata>{},
              std::get<0>(*callback.result()));
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kNoRecognizedCredential);
    authenticator_state_->registrations.clear();
  }
  {
    // A non-resident credential that matches the allow-list.
    std::vector<uint8_t> credential_id = {1, 2, 3, 4};
    ASSERT_TRUE(authenticator_state_->InjectRegistration(credential_id, kRpId));
    CredentialInfoCallback callback;
    CtapGetAssertionRequest allow_list_request = request;
    allow_list_request.allow_list.emplace_back(
        CredentialType::kPublicKey, std::vector<uint8_t>{1, 2, 3, 4});
    authenticator_->GetPlatformCredentialInfoForRequest(
        allow_list_request, CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    DiscoverableCredentialMetadata expected = DiscoverableCredentialMetadata(
        AuthenticatorType::kOther, kRpId, credential_id,
        PublicKeyCredentialUserEntity());
    EXPECT_THAT(std::get<0>(*callback.result()),
                testing::UnorderedElementsAre(expected));
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
    authenticator_state_->registrations.clear();
  }
  {
    // A set of matching resident credentials.
    PublicKeyCredentialUserEntity user1(std::vector<uint8_t>{1, 2, 3, 4});
    PublicKeyCredentialUserEntity user2(std::vector<uint8_t>{5, 6, 7, 8});
    std::vector<uint8_t> id1 = {1, 2, 3, 4};
    std::vector<uint8_t> id2 = {5, 6, 7, 8};
    ASSERT_TRUE(authenticator_state_->InjectResidentKey(
        id1, PublicKeyCredentialRpEntity(kRpId), user1));
    ASSERT_TRUE(authenticator_state_->InjectResidentKey(
        id2, PublicKeyCredentialRpEntity(kRpId), user2));
    CredentialInfoCallback callback;
    authenticator_->GetPlatformCredentialInfoForRequest(
        request, CtapGetAssertionOptions(), callback.callback());
    callback.WaitForCallback();
    DiscoverableCredentialMetadata expected1 = DiscoverableCredentialMetadata(
        AuthenticatorType::kOther, kRpId, id1, user1);
    DiscoverableCredentialMetadata expected2 = DiscoverableCredentialMetadata(
        AuthenticatorType::kOther, kRpId, id2, user2);
    EXPECT_THAT(std::get<0>(*callback.result()),
                testing::UnorderedElementsAre(expected1, expected2));
    EXPECT_EQ(
        std::get<1>(*callback.result()),
        FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
    authenticator_state_->registrations.clear();
  }
}

}  // namespace

}  // namespace device
