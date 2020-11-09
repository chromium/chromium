// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_ctap2_device.h"

#include <memory>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/cbor/reader.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/test_callback_receiver.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using TestCallbackReceiver =
    test::ValueCallbackReceiver<base::Optional<std::vector<uint8_t>>>;

void SendCommand(VirtualCtap2Device* device,
                 base::span<const uint8_t> command,
                 FidoDevice::DeviceCallback callback = base::DoNothing()) {
  device->DeviceTransact(fido_parsing_utils::Materialize(command),
                         std::move(callback));
}

// DecodeCBOR parses a CBOR structure, ignoring the first byte of |in|, which is
// assumed to be a CTAP2 status byte.
base::Optional<cbor::Value> DecodeCBOR(base::span<const uint8_t> in) {
  CHECK(!in.empty());
  return cbor::Reader::Read(in.subspan(1));
}

}  // namespace

class VirtualCtap2DeviceTest : public ::testing::Test {
 protected:
  void MakeDevice() { device_ = std::make_unique<VirtualCtap2Device>(); }

  void MakeSelfDestructingDevice() {
    MakeDevice();
    device_->mutable_state()->simulate_press_callback =
        base::BindLambdaForTesting([&](VirtualFidoDevice* _) {
          device_.reset();
          return true;
        });
  }

  std::unique_ptr<VirtualCtap2Device> device_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(VirtualCtap2DeviceTest, ParseMakeCredentialRequestForVirtualCtapKey) {
  const auto& cbor_request = cbor::Reader::Read(
      base::make_span(test_data::kCtapMakeCredentialRequest).subspan(1));
  ASSERT_TRUE(cbor_request);
  ASSERT_TRUE(cbor_request->is_map());
  const base::Optional<CtapMakeCredentialRequest> request =
      CtapMakeCredentialRequest::Parse(cbor_request->GetMap());
  ASSERT_TRUE(request);
  EXPECT_THAT(request->client_data_hash,
              ::testing::ElementsAreArray(test_data::kClientDataHash));
  EXPECT_EQ(test_data::kRelyingPartyId, request->rp.id);
  EXPECT_EQ("Acme", request->rp.name);
  EXPECT_THAT(request->user.id,
              ::testing::ElementsAreArray(test_data::kUserId));
  ASSERT_TRUE(request->user.name);
  EXPECT_EQ("johnpsmith@example.com", *request->user.name);
  ASSERT_TRUE(request->user.display_name);
  EXPECT_EQ("John P. Smith", *request->user.display_name);
  ASSERT_TRUE(request->user.icon_url);
  EXPECT_EQ("https://pics.acme.com/00/p/aBjjjpqPb.png",
            request->user.icon_url->spec());
  ASSERT_EQ(2u,
            request->public_key_credential_params.public_key_credential_params()
                .size());
  EXPECT_EQ(-7,
            request->public_key_credential_params.public_key_credential_params()
                .at(0)
                .algorithm);
  EXPECT_EQ(257,
            request->public_key_credential_params.public_key_credential_params()
                .at(1)
                .algorithm);
  EXPECT_EQ(UserVerificationRequirement::kRequired, request->user_verification);
  EXPECT_TRUE(request->resident_key_required);
}

TEST_F(VirtualCtap2DeviceTest, ParseGetAssertionRequestForVirtualCtapKey) {
  constexpr uint8_t kAllowedCredentialOne[] = {
      0xf2, 0x20, 0x06, 0xde, 0x4f, 0x90, 0x5a, 0xf6, 0x8a, 0x43, 0x94,
      0x2f, 0x02, 0x4f, 0x2a, 0x5e, 0xce, 0x60, 0x3d, 0x9c, 0x6d, 0x4b,
      0x3d, 0xf8, 0xbe, 0x08, 0xed, 0x01, 0xfc, 0x44, 0x26, 0x46, 0xd0,
      0x34, 0x85, 0x8a, 0xc7, 0x5b, 0xed, 0x3f, 0xd5, 0x80, 0xbf, 0x98,
      0x08, 0xd9, 0x4f, 0xcb, 0xee, 0x82, 0xb9, 0xb2, 0xef, 0x66, 0x77,
      0xaf, 0x0a, 0xdc, 0xc3, 0x58, 0x52, 0xea, 0x6b, 0x9e};
  constexpr uint8_t kAllowedCredentialTwo[] = {
      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
      0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03};

  const auto& cbor_request = cbor::Reader::Read(
      base::make_span(test_data::kTestComplexCtapGetAssertionRequest)
          .subspan(1));
  ASSERT_TRUE(cbor_request);
  ASSERT_TRUE(cbor_request->is_map());

  const base::Optional<CtapGetAssertionRequest> request =
      CtapGetAssertionRequest::Parse(cbor_request->GetMap());
  EXPECT_THAT(request->client_data_hash,
              ::testing::ElementsAreArray(test_data::kClientDataHash));
  EXPECT_EQ(test_data::kRelyingPartyId, request->rp_id);
  EXPECT_EQ(UserVerificationRequirement::kRequired, request->user_verification);
  EXPECT_FALSE(request->user_presence_required);
  ASSERT_EQ(2u, request->allow_list.size());

  EXPECT_THAT(request->allow_list.at(0).id(),
              ::testing::ElementsAreArray(kAllowedCredentialOne));
  EXPECT_THAT(request->allow_list.at(1).id(),
              ::testing::ElementsAreArray(kAllowedCredentialTwo));
}

// Tests that destroying the virtual device from the |simulate_press_callback|
// does not crash.
TEST_F(VirtualCtap2DeviceTest, DestroyInsideSimulatePressCallback) {
  MakeSelfDestructingDevice();
  SendCommand(device_.get(), test_data::kCtapSimpleMakeCredentialRequest);
  ASSERT_FALSE(device_);

  MakeSelfDestructingDevice();
  SendCommand(device_.get(), test_data::kCtapGetAssertionRequest);
  ASSERT_FALSE(device_);
}

// Tests that the attestation certificate returned on MakeCredential is valid.
// See
// https://w3c.github.io/webauthn/#sctn-packed-attestation-cert-requirements
TEST_F(VirtualCtap2DeviceTest, AttestationCertificateIsValid) {
  MakeDevice();
  TestCallbackReceiver callback_receiver;
  SendCommand(device_.get(), test_data::kCtapSimpleMakeCredentialRequest,
              callback_receiver.callback());
  callback_receiver.WaitForCallback();

  base::Optional<cbor::Value> cbor = DecodeCBOR(*callback_receiver.value());
  ASSERT_TRUE(cbor);
  base::Optional<AuthenticatorMakeCredentialResponse> response =
      ReadCTAPMakeCredentialResponse(
          FidoTransportProtocol::kUsbHumanInterfaceDevice, std::move(cbor));
  ASSERT_TRUE(response);

  const AttestationStatement& attestation =
      response->attestation_object().attestation_statement();

  EXPECT_FALSE(attestation.IsSelfAttestation());
  EXPECT_FALSE(
      attestation.IsAttestationCertificateInappropriatelyIdentifying());

  base::span<const uint8_t> cert_bytes = *attestation.GetLeafCertificate();
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBytes(
          reinterpret_cast<const char*>(cert_bytes.data()), cert_bytes.size());
  ASSERT_TRUE(cert);

  const auto& subject = cert->subject();
  EXPECT_EQ("Batch Certificate", subject.common_name);
  EXPECT_EQ("US", subject.country_name);
  EXPECT_THAT(subject.organization_names, testing::ElementsAre("Chromium"));
  EXPECT_THAT(subject.organization_unit_names,
              testing::ElementsAre("Authenticator Attestation"));

  base::Time now = base::Time::Now();
  EXPECT_LT(cert->valid_start(), now);
  EXPECT_GT(cert->valid_expiry(), now);

  bool present;
  bool critical;
  base::StringPiece contents;
  ASSERT_TRUE(net::asn1::ExtractExtensionFromDERCert(
      net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()),
      base::StringPiece("\x55\x1d\x13"), &present, &critical, &contents));
  EXPECT_TRUE(present);
  EXPECT_TRUE(critical);
  EXPECT_EQ(base::StringPiece("\x30\x03\x01\x01\x00", 5), contents);
}

}  // namespace device
