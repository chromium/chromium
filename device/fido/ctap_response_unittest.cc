// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/attestation_statement_formats.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/opaque_attestation_statement.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"
#include "fido_transport_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

// clang-format off
constexpr uint8_t kTestAuthenticatorGetInfoResponseWithNoVersion[] = {
    // Success status byte
    0x00,
    // Map of 6 elements
    0xA6,
    // Key(01) - versions
    0x01,
    // Array(0)
    0x80,
    // Key(02) - extensions
    0x02,
    // Array(2)
    0x82,
    // "uvm"
    0x63, 0x75, 0x76, 0x6D,
    // "hmac-secret"
    0x6B, 0x68, 0x6D, 0x61, 0x63, 0x2D, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74,
    // Key(03) - AAGUID
    0x03,
    // Bytes(16)
    0x50, 0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17,
    0x11, 0x1F, 0x9E, 0xDC, 0x7D,
    // Key(04) - options
    0x04,
    // Map(05)
    0xA5,
    // Key - "rk"
    0x62, 0x72, 0x6B,
    // true
    0xF5,
    // Key - "up"
    0x62, 0x75, 0x70,
    // true
    0xF5,
    // Key - "uv"
    0x62, 0x75, 0x76,
    // true
    0xF5,
    // Key - "plat"
    0x64, 0x70, 0x6C, 0x61, 0x74,
    // true
    0xF5,
    // Key - "clientPin"
    0x69, 0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x50, 0x69, 0x6E,
    // false
    0xF4,
    // Key(05) - Max message size
    0x05,
    // 1200
    0x19, 0x04, 0xB0,
    // Key(06) - Pin protocols
    0x06,
    // Array[1]
    0x81, 0x01,
};

constexpr uint8_t kTestAuthenticatorGetInfoResponseWithDuplicateVersion[] = {
    // Success status byte
    0x00,
    // Map of 6 elements
    0xA6,
    // Key(01) - versions
    0x01,
    // Array(03)
    0x83,
    // "U2F_V9"
    0x66, 0x55, 0x32, 0x46, 0x5F, 0x56, 0x39,
    // "U2F_V9"
    0x66, 0x55, 0x32, 0x46, 0x5F, 0x56, 0x39,
    // "U2F_V2"
    0x66, 0x55, 0x32, 0x46, 0x5F, 0x56, 0x32,
    // Key(02) - extensions
    0x02,
    // Array(2)
    0x82,
    // "uvm"
    0x63, 0x75, 0x76, 0x6D,
    // "hmac-secret"
    0x6B, 0x68, 0x6D, 0x61, 0x63, 0x2D, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74,
    // Key(03) - AAGUID
    0x03,
    // Bytes(16)
    0x50, 0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17,
    0x11, 0x1F, 0x9E, 0xDC, 0x7D,
    // Key(04) - options
    0x04,
    // Map(05)
    0xA5,
    // Key - "rk"
    0x62, 0x72, 0x6B,
    // true
    0xF5,
    // Key - "up"
    0x62, 0x75, 0x70,
    // true
    0xF5,
    // Key - "uv"
    0x62, 0x75, 0x76,
    // true
    0xF5,
    // Key - "plat"
    0x64, 0x70, 0x6C, 0x61, 0x74,
    // true
    0xF5,
    // Key - "clientPin"
    0x69, 0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x50, 0x69, 0x6E,
    // false
    0xF4,
    // Key(05) - Max message size
    0x05,
    // 1200
    0x19, 0x04, 0xB0,
    // Key(06) - Pin protocols
    0x06,
    // Array[1]
    0x81, 0x01,
};

constexpr uint8_t kTestAuthenticatorGetInfoResponseWithCtap2_1[] = {
    // Success status byte
    0x00,
    // Map of 6 elements
    0xA6,
    // Key(01) - versions
    0x01,
    // Array(03)
    0x83,
    // "U2F_V2"
    0x66, 'U', '2', 'F', '_', 'V', '2',
    // "FIDO_2_0"
    0x68, 'F', 'I', 'D', 'O', '_', '2', '_', '0',
    // "FIDO_2_1"
    0x68, 'F', 'I', 'D', 'O', '_', '2', '_', '1',
    // Key(02) - extensions
    0x02,
    // Array(2)
    0x82,
    // "uvm"
    0x63, 0x75, 0x76, 0x6D,
    // "hmac-secret"
    0x6B, 0x68, 0x6D, 0x61, 0x63, 0x2D, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74,
    // Key(03) - AAGUID
    0x03,
    // Bytes(16)
    0x50, 0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17,
    0x11, 0x1F, 0x9E, 0xDC, 0x7D,
    // Key(04) - options
    0x04,
    // Map(05)
    0xA5,
    // Key - "rk"
    0x62, 0x72, 0x6B,
    // true
    0xF5,
    // Key - "up"
    0x62, 0x75, 0x70,
    // true
    0xF5,
    // Key - "uv"
    0x62, 0x75, 0x76,
    // true
    0xF5,
    // Key - "plat"
    0x64, 0x70, 0x6C, 0x61, 0x74,
    // true
    0xF5,
    // Key - "clientPin"
    0x69, 0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x50, 0x69, 0x6E,
    // false
    0xF4,
    // Key(05) - Max message size
    0x05,
    // 1200
    0x19, 0x04, 0xB0,
    // Key(06) - Pin protocols
    0x06,
    // Array[1]
    0x81, 0x01,
};

constexpr uint8_t kTestAuthenticatorGetInfoResponseOnlyCtap2_1[] = {
    // Success status byte
    0x00,
    // Map of 6 elements
    0xA6,
    // Key(01) - versions
    0x01,
    // Array(01)
    0x81,
    // "FIDO_2_1"
    0x68, 'F', 'I', 'D', 'O', '_', '2', '_', '1',
    // Key(02) - extensions
    0x02,
    // Array(2)
    0x82,
    // "uvm"
    0x63, 0x75, 0x76, 0x6D,
    // "hmac-secret"
    0x6B, 0x68, 0x6D, 0x61, 0x63, 0x2D, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74,
    // Key(03) - AAGUID
    0x03,
    // Bytes(16)
    0x50, 0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17,
    0x11, 0x1F, 0x9E, 0xDC, 0x7D,
    // Key(04) - options
    0x04,
    // Map(05)
    0xA5,
    // Key - "rk"
    0x62, 0x72, 0x6B,
    // true
    0xF5,
    // Key - "up"
    0x62, 0x75, 0x70,
    // true
    0xF5,
    // Key - "uv"
    0x62, 0x75, 0x76,
    // true
    0xF5,
    // Key - "plat"
    0x64, 0x70, 0x6C, 0x61, 0x74,
    // true
    0xF5,
    // Key - "clientPin"
    0x69, 0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x50, 0x69, 0x6E,
    // false
    0xF4,
    // Key(05) - Max message size
    0x05,
    // 1200
    0x19, 0x04, 0xB0,
    // Key(06) - Pin protocols
    0x06,
    // Array[1]
    0x81, 0x01,
};

constexpr uint8_t kTestAuthenticatorGetInfoResponseWithIncorrectAaguid[] = {
    // Success status byte
    0x00,
    // Map of 6 elements
    0xA6,
    // Key(01) - versions
    0x01,
    // Array(01)
    0x81,
    // "U2F_V2"
    0x66, 0x55, 0x32, 0x46, 0x5F, 0x56, 0x32,
    // Key(02) - extensions
    0x02,
    // Array(2)
    0x82,
    // "uvm"
    0x63, 0x75, 0x76, 0x6D,
    // "hmac-secret"
    0x6B, 0x68, 0x6D, 0x61, 0x63, 0x2D, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74,
    // Key(03) - AAGUID
    0x03,
    // Bytes(17) - FIDO2 device AAGUID must be 16 bytes long in order to be
    // correct.
    0x51, 0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17,
    0x11, 0x1F, 0x9E, 0xDC, 0x7D, 0x00,
    // Key(04) - options
    0x04,
    // Map(05)
    0xA5,
    // Key - "rk"
    0x62, 0x72, 0x6B,
    // true
    0xF5,
    // Key - "up"
    0x62, 0x75, 0x70,
    // true
    0xF5,
    // Key - "uv"
    0x62, 0x75, 0x76,
    // true
    0xF5,
    // Key - "plat"
    0x64, 0x70, 0x6C, 0x61, 0x74,
    // true
    0xF5,
    // Key - "clientPin"
    0x69, 0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x50, 0x69, 0x6E,
    // false
    0xF4,
    // Key(05) - Max message size
    0x05,
    // 1200
    0x19, 0x04, 0xB0,
    // Key(06) - Pin protocols
    0x06,
    // Array[1]
    0x81, 0x01,
};

// The attested credential data, excluding the public key bytes. Append
// with kTestECPublicKeyCOSE to get the complete attestation data.
constexpr uint8_t kTestAttestedCredentialDataPrefix[] = {
    // 16-byte aaguid
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    // 2-byte length
    0x00, 0x40,
    // 64-byte key handle
    0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C, 0x26,
    0x35, 0xEF, 0xAA, 0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17, 0x36, 0xC3,
    0x71, 0x7D, 0xA4, 0x85, 0x34, 0xC8, 0xC6, 0xB6, 0x54, 0xD7, 0xFF, 0x94,
    0x5F, 0x50, 0xB5, 0xCC, 0x4E, 0x78, 0x05, 0x5B, 0xDD, 0x39, 0x6B, 0x64,
    0xF7, 0x8D, 0xA2, 0xC5, 0xF9, 0x62, 0x00, 0xCC, 0xD4, 0x15, 0xCD, 0x08,
    0xFE, 0x42, 0x00, 0x38,
};

// The authenticator data, excluding the attested credential data bytes. Append
// with attested credential data to get the complete authenticator data.
constexpr uint8_t kTestAuthenticatorDataPrefix[] = {
    // sha256 hash of rp id.
    0x11, 0x94, 0x22, 0x8D, 0xA8, 0xFD, 0xBD, 0xEE, 0xFD, 0x26, 0x1B, 0xD7,
    0xB6, 0x59, 0x5C, 0xFD, 0x70, 0xA5, 0x0D, 0x70, 0xC6, 0x40, 0x7B, 0xCF,
    0x01, 0x3D, 0xE9, 0x6D, 0x4E, 0xFB, 0x17, 0xDE,
    // flags (TUP and AT bits set)
    0x41,
    // counter
    0x00, 0x00, 0x00, 0x00};

// Components of the CBOR needed to form an authenticator object.
// Combined diagnostic notation:
// {"fmt": "fido-u2f", "attStmt": {"sig": h'30...}, "authData": h'D4C9D9...'}
constexpr uint8_t kFormatFidoU2fCBOR[] = {
    // map(3)
    0xA3,
    // text(3)
    0x63,
    // "fmt"
    0x66, 0x6D, 0x74,
    // text(8)
    0x68,
    // "fido-u2f"
    0x66, 0x69, 0x64, 0x6F, 0x2D, 0x75, 0x32, 0x66};

constexpr uint8_t kAttStmtCBOR[] = {
    // text(7)
    0x67,
    // "attStmt"
    0x61, 0x74, 0x74, 0x53, 0x74, 0x6D, 0x74};

constexpr uint8_t kAuthDataCBOR[] = {
    // text(8)
    0x68,
    // "authData"
    0x61, 0x75, 0x74, 0x68, 0x44, 0x61, 0x74, 0x61,
    // bytes(196). i.e., the authenticator_data byte array corresponding to
    // kTestAuthenticatorDataPrefix|, |kTestAttestedCredentialDataPrefix|,
    // and test_data::kTestECPublicKeyCOSE.
    0x58, 0xC4};
// clang-format on

constexpr std::array<uint8_t, kAaguidLength> kTestDeviceAaguid = {
    {0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17, 0x11,
     0x1F, 0x9E, 0xDC, 0x7D}};

std::vector<uint8_t> GetTestAttestedCredentialDataBytes() {
  // Combine kTestAttestedCredentialDataPrefix and kTestECPublicKeyCOSE.
  auto test_attested_data =
      fido_parsing_utils::Materialize(kTestAttestedCredentialDataPrefix);
  fido_parsing_utils::Append(&test_attested_data,
                             test_data::kTestECPublicKeyCOSE);
  return test_attested_data;
}

std::vector<uint8_t> GetTestAuthenticatorDataBytes() {
  // Build the test authenticator data.
  auto test_authenticator_data =
      fido_parsing_utils::Materialize(kTestAuthenticatorDataPrefix);
  auto test_attested_data = GetTestAttestedCredentialDataBytes();
  fido_parsing_utils::Append(&test_authenticator_data, test_attested_data);
  return test_authenticator_data;
}

std::vector<uint8_t> GetTestAttestationObjectBytes() {
  auto test_authenticator_object =
      fido_parsing_utils::Materialize(kFormatFidoU2fCBOR);
  fido_parsing_utils::Append(&test_authenticator_object, kAttStmtCBOR);
  fido_parsing_utils::Append(&test_authenticator_object,
                             test_data::kU2fAttestationStatementCBOR);
  fido_parsing_utils::Append(&test_authenticator_object, kAuthDataCBOR);
  auto test_authenticator_data = GetTestAuthenticatorDataBytes();
  fido_parsing_utils::Append(&test_authenticator_object,
                             test_authenticator_data);
  return test_authenticator_object;
}

std::vector<uint8_t> GetTestSignResponse() {
  return fido_parsing_utils::Materialize(test_data::kTestU2fSignResponse);
}

// Get a subset of the response for testing error handling.
std::vector<uint8_t> GetTestCorruptedSignResponse(size_t length) {
  DCHECK_LE(length, std::size(test_data::kTestU2fSignResponse));
  return fido_parsing_utils::Materialize(fido_parsing_utils::ExtractSpan(
      test_data::kTestU2fSignResponse, 0, length));
}

// Return a key handle used for GetAssertion request.
std::vector<uint8_t> GetTestCredentialRawIdBytes() {
  return fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle);
}

// DecodeCBOR parses a CBOR structure, ignoring the first byte of |in|, which is
// assumed to be a CTAP2 status byte.
std::optional<cbor::Value> DecodeCBOR(base::span<const uint8_t> in) {
  CHECK(!in.empty());
  return cbor::Reader::Read(in.subspan(1));
}

}  // namespace

// Leveraging example 4 of section 6.1 of the spec https://fidoalliance.org
// /specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-
// 20170927.html
TEST(CTAPResponseTest, TestReadMakeCredentialResponse) {
  auto make_credential_response = ReadCTAPMakeCredentialResponse(
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      DecodeCBOR(test_data::kTestMakeCredentialResponse));
  ASSERT_TRUE(make_credential_response);
  auto cbor_attestation_object = cbor::Reader::Read(
      make_credential_response->GetCBOREncodedAttestationObject());
  ASSERT_TRUE(cbor_attestation_object);
  ASSERT_TRUE(cbor_attestation_object->is_map());

  const auto& attestation_object_map = cbor_attestation_object->GetMap();
  auto it = attestation_object_map.find(cbor::Value(kFormatKey));
  ASSERT_TRUE(it != attestation_object_map.end());
  ASSERT_TRUE(it->second.is_string());
  EXPECT_EQ(it->second.GetString(), "packed");

  it = attestation_object_map.find(cbor::Value(kAuthDataKey));
  ASSERT_TRUE(it != attestation_object_map.end());
  ASSERT_TRUE(it->second.is_bytestring());
  EXPECT_THAT(
      it->second.GetBytestring(),
      ::testing::ElementsAreArray(test_data::kCtap2MakeCredentialAuthData));

  it = attestation_object_map.find(cbor::Value(kAttestationStatementKey));
  ASSERT_TRUE(it != attestation_object_map.end());
  ASSERT_TRUE(it->second.is_map());

  const auto& attestation_statement_map = it->second.GetMap();
  auto attStmt_it = attestation_statement_map.find(cbor::Value("alg"));

  ASSERT_TRUE(attStmt_it != attestation_statement_map.end());
  ASSERT_TRUE(attStmt_it->second.is_integer());
  EXPECT_EQ(attStmt_it->second.GetInteger(), -7);

  attStmt_it = attestation_statement_map.find(cbor::Value("sig"));
  ASSERT_TRUE(attStmt_it != attestation_statement_map.end());
  ASSERT_TRUE(attStmt_it->second.is_bytestring());
  EXPECT_THAT(
      attStmt_it->second.GetBytestring(),
      ::testing::ElementsAreArray(test_data::kCtap2MakeCredentialSignature));

  attStmt_it = attestation_statement_map.find(cbor::Value("x5c"));
  ASSERT_TRUE(attStmt_it != attestation_statement_map.end());
  const auto& certificate = attStmt_it->second;
  ASSERT_TRUE(certificate.is_array());
  ASSERT_EQ(certificate.GetArray().size(), 1u);
  ASSERT_TRUE(certificate.GetArray()[0].is_bytestring());
  EXPECT_THAT(
      certificate.GetArray()[0].GetBytestring(),
      ::testing::ElementsAreArray(test_data::kCtap2MakeCredentialCertificate));
  EXPECT_THAT(
      make_credential_response->attestation_object.GetCredentialId(),
      ::testing::ElementsAreArray(test_data::kCtap2MakeCredentialCredentialId));
}

TEST(CTAPResponseTest, TestMakeCredentialNoneAttestationResponse) {
  auto make_credential_response = ReadCTAPMakeCredentialResponse(
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      DecodeCBOR(test_data::kTestMakeCredentialResponse));
  ASSERT_TRUE(make_credential_response);
  make_credential_response->attestation_object.EraseAttestationStatement(
      AttestationObject::AAGUID::kErase);
  EXPECT_THAT(make_credential_response->GetCBOREncodedAttestationObject(),
              ::testing::ElementsAreArray(test_data::kNoneAttestationResponse));
}

// Leveraging example 5 of section 6.1 of the CTAP spec.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html
TEST(CTAPResponseTest, TestReadGetAssertionResponse) {
  auto get_assertion_response = ReadCTAPGetAssertionResponse(
      FidoTransportProtocol::kBluetoothLowEnergy,
      DecodeCBOR(test_data::kDeviceGetAssertionResponse));
  ASSERT_TRUE(get_assertion_response);
  EXPECT_EQ(*get_assertion_response->transport_used,
            FidoTransportProtocol::kBluetoothLowEnergy);
  ASSERT_TRUE(get_assertion_response->num_credentials);
  EXPECT_EQ(*get_assertion_response->num_credentials, 1u);

  EXPECT_THAT(
      get_assertion_response->authenticator_data.SerializeToByteArray(),
      ::testing::ElementsAreArray(test_data::kCtap2GetAssertionAuthData));
  EXPECT_THAT(
      get_assertion_response->signature,
      ::testing::ElementsAreArray(test_data::kCtap2GetAssertionSignature));
}

// Test that U2F register response is properly parsed.
TEST(CTAPResponseTest, TestParseRegisterResponseData) {
  auto response =
      AuthenticatorMakeCredentialResponse::CreateFromU2fRegisterResponse(
          FidoTransportProtocol::kUsbHumanInterfaceDevice,
          test_data::kApplicationParameter,
          test_data::kTestU2fRegisterResponse);
  ASSERT_TRUE(response);
  EXPECT_THAT(response->attestation_object.GetCredentialId(),
              ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
  EXPECT_EQ(GetTestAttestationObjectBytes(),
            response->GetCBOREncodedAttestationObject());
}

// These test the parsing of the U2F raw bytes of the registration response.
// Test that an EC public key serializes to CBOR properly.
TEST(CTAPResponseTest, TestSerializedPublicKey) {
  auto public_key = P256PublicKey::ExtractFromU2fRegistrationResponse(
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256),
      test_data::kTestU2fRegisterResponse);
  ASSERT_TRUE(public_key);
  EXPECT_THAT(public_key->cose_key_bytes,
              ::testing::ElementsAreArray(test_data::kTestECPublicKeyCOSE));
}

// Test that the attestation statement cbor map is constructed properly.
TEST(CTAPResponseTest, TestParseU2fAttestationStatementCBOR) {
  auto fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(
          test_data::kTestU2fRegisterResponse);
  ASSERT_TRUE(fido_attestation_statement);
  auto cbor = cbor::Writer::Write(AsCBOR(*fido_attestation_statement));
  ASSERT_TRUE(cbor);
  EXPECT_THAT(*cbor, ::testing::ElementsAreArray(
                         test_data::kU2fAttestationStatementCBOR));
}

// Tests that well-formed attested credential data serializes properly.
TEST(CTAPResponseTest, TestSerializeAttestedCredentialData) {
  auto public_key = P256PublicKey::ExtractFromU2fRegistrationResponse(
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256),
      test_data::kTestU2fRegisterResponse);
  auto attested_data = AttestedCredentialData::CreateFromU2fRegisterResponse(
      test_data::kTestU2fRegisterResponse, std::move(public_key));
  ASSERT_TRUE(attested_data);
  EXPECT_EQ(GetTestAttestedCredentialDataBytes(),
            attested_data->SerializeAsBytes());
}

// Tests that well-formed authenticator data serializes properly.
TEST(CTAPResponseTest, TestSerializeAuthenticatorData) {
  auto public_key = P256PublicKey::ExtractFromU2fRegistrationResponse(
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256),
      test_data::kTestU2fRegisterResponse);
  auto attested_data = AttestedCredentialData::CreateFromU2fRegisterResponse(
      test_data::kTestU2fRegisterResponse, std::move(public_key));

  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);

  AuthenticatorData authenticator_data(test_data::kApplicationParameter, flags,
                                       std::array<uint8_t, 4>{} /* counter */,
                                       std::move(attested_data));

  EXPECT_EQ(GetTestAuthenticatorDataBytes(),
            authenticator_data.SerializeToByteArray());
}

// Tests that a U2F attestation object serializes properly.
TEST(CTAPResponseTest, TestSerializeU2fAttestationObject) {
  auto public_key = P256PublicKey::ExtractFromU2fRegistrationResponse(
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256),
      test_data::kTestU2fRegisterResponse);
  auto attested_data = AttestedCredentialData::CreateFromU2fRegisterResponse(
      test_data::kTestU2fRegisterResponse, std::move(public_key));

  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);
  AuthenticatorData authenticator_data(test_data::kApplicationParameter, flags,
                                       std::array<uint8_t, 4>{} /* counter */,
                                       std::move(attested_data));

  // Construct the attestation statement.
  auto fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(
          test_data::kTestU2fRegisterResponse);

  // Construct the attestation object.
  auto attestation_object = std::make_unique<AttestationObject>(
      std::move(authenticator_data), std::move(fido_attestation_statement));

  ASSERT_TRUE(attestation_object);
  EXPECT_EQ(GetTestAttestationObjectBytes(),
            cbor::Writer::Write(AsCBOR(*attestation_object))
                .value_or(std::vector<uint8_t>()));
}

// Tests that U2F authenticator data is properly serialized.
TEST(CTAPResponseTest, TestSerializeAuthenticatorDataForSign) {
  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence);

  EXPECT_THAT(
      AuthenticatorData(test_data::kApplicationParameter, flags,
                        test_data::kTestSignatureCounter, std::nullopt)
          .SerializeToByteArray(),
      ::testing::ElementsAreArray(test_data::kTestSignAuthenticatorData));
}

TEST(CTAPResponseTest, TestParseSignResponseData) {
  auto response = AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
      test_data::kApplicationParameter, GetTestSignResponse(),
      GetTestCredentialRawIdBytes(),
      FidoTransportProtocol::kUsbHumanInterfaceDevice);
  ASSERT_TRUE(response);
  EXPECT_EQ(GetTestCredentialRawIdBytes(), response->credential->id);
  EXPECT_THAT(
      response->authenticator_data.SerializeToByteArray(),
      ::testing::ElementsAreArray(test_data::kTestSignAuthenticatorData));
  EXPECT_THAT(response->signature,
              ::testing::ElementsAreArray(test_data::kU2fSignature));
  EXPECT_EQ(response->transport_used,
            FidoTransportProtocol::kUsbHumanInterfaceDevice);
}

TEST(CTAPResponseTest, TestParseU2fSignWithNullNullKeyHandle) {
  auto response = AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
      test_data::kApplicationParameter, GetTestSignResponse(),
      std::vector<uint8_t>(), FidoTransportProtocol::kUsbHumanInterfaceDevice);
  EXPECT_FALSE(response);
}

TEST(CTAPResponseTest, TestParseU2fSignWithNullResponse) {
  auto response = AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
      test_data::kApplicationParameter, std::vector<uint8_t>(),
      GetTestCredentialRawIdBytes(),
      FidoTransportProtocol::kUsbHumanInterfaceDevice);
  EXPECT_FALSE(response);
}

TEST(CTAPResponseTest, TestParseU2fSignWithCTAP2Flags) {
  std::vector<uint8_t> sign_response = GetTestSignResponse();
  // Set two flags that should only be set in CTAP2 responses and expect parsing
  // to fail.
  sign_response[0] |=
      static_cast<uint8_t>(AuthenticatorData::Flag::kExtensionDataIncluded);
  sign_response[0] |=
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);

  auto response = AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
      test_data::kApplicationParameter, sign_response,
      GetTestCredentialRawIdBytes(),
      FidoTransportProtocol::kUsbHumanInterfaceDevice);
  EXPECT_FALSE(response);
}

TEST(CTAPResponseTest, TestParseU2fSignWithNullCorruptedCounter) {
  // A sign response of less than 5 bytes.
  auto response = AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
      test_data::kApplicationParameter, GetTestCorruptedSignResponse(3),
      GetTestCredentialRawIdBytes(),
      FidoTransportProtocol::kUsbHumanInterfaceDevice);
  EXPECT_FALSE(response);
}

TEST(CTAPResponseTest, TestParseU2fSignWithNullCorruptedSignature) {
  // A sign response no more than 5 bytes.
  auto response = AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
      test_data::kApplicationParameter, GetTestCorruptedSignResponse(5),
      GetTestCredentialRawIdBytes(),
      FidoTransportProtocol::kUsbHumanInterfaceDevice);
  EXPECT_FALSE(response);
}

TEST(CTAPResponseTest, TestReadGetInfoResponse) {
  auto get_info_response =
      ReadCTAPGetInfoResponse(test_data::kTestGetInfoResponsePlatformDevice);
  ASSERT_TRUE(get_info_response);
  ASSERT_TRUE(get_info_response->max_msg_size);
  EXPECT_EQ(*get_info_response->max_msg_size, 1200u);
  EXPECT_TRUE(
      base::Contains(get_info_response->versions, ProtocolVersion::kCtap2));
  EXPECT_TRUE(
      base::Contains(get_info_response->versions, ProtocolVersion::kU2f));
  EXPECT_EQ(get_info_response->ctap2_versions.size(), 1u);
  EXPECT_TRUE(base::Contains(get_info_response->ctap2_versions,
                             Ctap2Version::kCtap2_0));
  EXPECT_EQ(get_info_response->options.is_platform_device,
            AuthenticatorSupportedOptions::PlatformDevice::kYes);
  EXPECT_TRUE(get_info_response->options.supports_resident_key);
  EXPECT_TRUE(get_info_response->options.supports_user_presence);
  EXPECT_EQ(AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedAndConfigured,
            get_info_response->options.user_verification_availability);
  EXPECT_EQ(AuthenticatorSupportedOptions::ClientPinAvailability::
                kSupportedButPinNotSet,
            get_info_response->options.client_pin_availability);
}

TEST(CTAPResponseTest, TestReadGetInfoResponseWithDuplicateVersion) {
  uint8_t
      get_info[sizeof(kTestAuthenticatorGetInfoResponseWithDuplicateVersion)];
  memcpy(get_info, kTestAuthenticatorGetInfoResponseWithDuplicateVersion,
         sizeof(get_info));
  // Should fail to parse with duplicate versions.
  EXPECT_FALSE(ReadCTAPGetInfoResponse(get_info));

  // Find the first of the duplicate versions and change it to a different
  // value. That should be sufficient to make the data parsable.
  static constexpr std::string_view kU2Fv9 = "U2F_V9";
  uint8_t* first_version = base::ranges::search(get_info, kU2Fv9);
  ASSERT_TRUE(first_version);
  memcpy(first_version, "U2F_V3", 6);
  std::optional<AuthenticatorGetInfoResponse> response =
      ReadCTAPGetInfoResponse(get_info);
  ASSERT_TRUE(response);
  EXPECT_EQ(1u, response->versions.size());
  EXPECT_TRUE(response->versions.contains(ProtocolVersion::kU2f));
  EXPECT_EQ(response->ctap2_versions.size(), 0u);
}

TEST(CTAPResponseTest, TestReadGetInfoResponseWithCtap2_1) {
  auto response =
      ReadCTAPGetInfoResponse(kTestAuthenticatorGetInfoResponseWithCtap2_1);
  ASSERT_TRUE(response);
  EXPECT_EQ(2u, response->versions.size());
  EXPECT_TRUE(response->versions.contains(ProtocolVersion::kU2f));
  EXPECT_TRUE(response->versions.contains(ProtocolVersion::kCtap2));
  EXPECT_EQ(response->ctap2_versions.size(), 2u);
  EXPECT_TRUE(base::Contains(response->ctap2_versions, Ctap2Version::kCtap2_0));
  EXPECT_TRUE(base::Contains(response->ctap2_versions, Ctap2Version::kCtap2_1));
}

// Tests that an authenticator returning only the string "FIDO_2_1" is properly
// recognized as a CTAP 2.1 authenticator.
TEST(CTAPResponseTest, TestReadGetInfoResponseOnlyCtap2_1) {
  auto response =
      ReadCTAPGetInfoResponse(kTestAuthenticatorGetInfoResponseOnlyCtap2_1);
  ASSERT_TRUE(response);
  EXPECT_EQ(1u, response->versions.size());
  EXPECT_TRUE(response->versions.contains(ProtocolVersion::kCtap2));
  EXPECT_EQ(response->ctap2_versions.size(), 1u);
  EXPECT_TRUE(base::Contains(response->ctap2_versions, Ctap2Version::kCtap2_1));
}

TEST(CTAPResponseTest, TestReadGetInfoResponseWithIncorrectFormat) {
  EXPECT_FALSE(
      ReadCTAPGetInfoResponse(kTestAuthenticatorGetInfoResponseWithNoVersion));
  EXPECT_FALSE(ReadCTAPGetInfoResponse(
      kTestAuthenticatorGetInfoResponseWithIncorrectAaguid));
}

TEST(CTAPResponseTest, TestSerializeGetInfoResponse) {
  AuthenticatorGetInfoResponse response(
      {ProtocolVersion::kCtap2, ProtocolVersion::kU2f},
      {Ctap2Version::kCtap2_0}, kTestDeviceAaguid);
  response.extensions.emplace({std::string("uvm"), std::string("hmac-secret")});
  AuthenticatorSupportedOptions options;
  options.supports_resident_key = true;
  options.is_platform_device =
      AuthenticatorSupportedOptions::PlatformDevice::kYes;
  options.client_pin_availability = AuthenticatorSupportedOptions::
      ClientPinAvailability::kSupportedButPinNotSet;
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  response.options = std::move(options);
  response.max_msg_size = 1200;
  response.pin_protocols.emplace({PINUVAuthProtocol::kV1});
  response.algorithms.reset();

  EXPECT_THAT(AuthenticatorGetInfoResponse::EncodeToCBOR(response),
              ::testing::ElementsAreArray(
                  base::make_span(test_data::kTestGetInfoResponsePlatformDevice)
                      .subspan(1)));
}

TEST(CTAPResponseTest, TestSerializeMakeCredentialResponse) {
  constexpr uint8_t kCoseEncodedPublicKey[] = {
      // clang-format off
      // map(5)
      0xa5,
      0x01,  // unsigned(1) kty
      0x02,  // unsigned(2) EC2
      0x03,  // unsigned(3) alg
      0x26,  // negative(6) ES256
      0x20,  // negative(0) crv
      0x01,  // unsigned(1) P-256
      0x21,  // negative(1) x
      //        byte(32)
      0x58, 0x20, 0xf7, 0xc4, 0xf4, 0xa6, 0xf1, 0xd7, 0x95, 0x38, 0xdf, 0xa4,
      0xc9, 0xac, 0x50, 0x84, 0x8d, 0xf7, 0x08, 0xbc, 0x1c, 0x99, 0xf5, 0xe6,
      0x0e, 0x51, 0xb4, 0x2a, 0x52, 0x1b, 0x35, 0xd3, 0xb6, 0x9a,
      0x22,  // negative(2) y
      //        byte(32)
      0x58, 0x20, 0xde, 0x7b, 0x7d, 0x6c, 0xa5, 0x64, 0xe7, 0x0e, 0xa3, 0x21,
      0xa4, 0xd5, 0xd9, 0x6e, 0xa0, 0x0e, 0xf0, 0xe2, 0xdb, 0x89, 0xdd, 0x61,
      0xd4, 0x89, 0x4c, 0x15, 0xac, 0x58, 0x5b, 0xd2, 0x36, 0x84,
      // clang-format on
  };

  const auto application_parameter =
      base::make_span(test_data::kApplicationParameter)
          .subspan<0, kRpIdHashLength>();
  // Starting signature counter value set by example 4 of the CTAP spec. The
  // signature counter can start at any value but it should never decrease.
  // https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html
  std::array<uint8_t, kSignCounterLength> signature_counter = {
      {0x00, 0x00, 0x00, 0x0b}};
  auto flag =
      base::strict_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      base::strict_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);
  AttestedCredentialData attested_credential_data(
      kTestDeviceAaguid,
      std::array<uint8_t, kCredentialIdLengthLength>{
          {0x00, 0x10}} /* credential_id_length */,
      fido_parsing_utils::Materialize(
          test_data::kCtap2MakeCredentialCredentialId),
      std::make_unique<PublicKey>(
          static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256),
          kCoseEncodedPublicKey, std::nullopt));
  AuthenticatorData authenticator_data(application_parameter, flag,
                                       signature_counter,
                                       std::move(attested_credential_data));

  cbor::Value::MapValue attestation_map;
  attestation_map.emplace("alg", -7);
  attestation_map.emplace("sig", fido_parsing_utils::Materialize(
                                     test_data::kCtap2MakeCredentialSignature));
  cbor::Value::ArrayValue certificate_chain;
  certificate_chain.emplace_back(fido_parsing_utils::Materialize(
      test_data::kCtap2MakeCredentialCertificate));
  attestation_map.emplace("x5c", std::move(certificate_chain));
  AuthenticatorMakeCredentialResponse response(
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      AttestationObject(
          std::move(authenticator_data),
          std::make_unique<OpaqueAttestationStatement>(
              "packed", cbor::Value(std::move(attestation_map)))));
  EXPECT_THAT(
      AsCTAPStyleCBORBytes(response),
      ::testing::ElementsAreArray(
          base::make_span(test_data::kTestMakeCredentialResponse).subspan(1)));
}

TEST(CTAPResponseTest, AttestationObjectResponseFields) {
  static const std::vector<uint8_t> kInvalidAttestationObject = {1, 2, 3};
  const std::optional<AttestationObject::ResponseFields> invalid =
      AttestationObject::ParseForResponseFields(
          kInvalidAttestationObject, /*attestation_acceptable=*/false);
  EXPECT_FALSE(invalid.has_value());

  static const std::vector<uint8_t> kAttestationObjectBytes = {
      0xa3, 0x63, 0x66, 0x6d, 0x74, 0x66, 0x70, 0x61, 0x63, 0x6b, 0x65, 0x64,
      0x67, 0x61, 0x74, 0x74, 0x53, 0x74, 0x6d, 0x74, 0xa3, 0x63, 0x61, 0x6c,
      0x67, 0x26, 0x63, 0x73, 0x69, 0x67, 0x58, 0x46, 0x30, 0x44, 0x02, 0x20,
      0x05, 0xaa, 0x7b, 0xcb, 0x4f, 0x15, 0xc8, 0x3d, 0x3a, 0x0b, 0x57, 0x12,
      0xa8, 0xab, 0x8d, 0x60, 0x16, 0x9b, 0xfb, 0x91, 0x91, 0xfd, 0x1d, 0xe6,
      0x30, 0xab, 0xae, 0xe3, 0x71, 0xd6, 0xfb, 0x33, 0x02, 0x20, 0x30, 0xba,
      0x47, 0x7b, 0x38, 0x06, 0x89, 0xbc, 0x46, 0x1c, 0xa1, 0x60, 0x7e, 0x99,
      0x88, 0x85, 0x7f, 0x24, 0xf9, 0x82, 0xb7, 0xb5, 0x03, 0x8f, 0x92, 0x16,
      0x86, 0xd6, 0x10, 0x50, 0x9c, 0xc8, 0x63, 0x78, 0x35, 0x63, 0x81, 0x59,
      0x02, 0xdc, 0x30, 0x82, 0x02, 0xd8, 0x30, 0x82, 0x01, 0xc0, 0xa0, 0x03,
      0x02, 0x01, 0x02, 0x02, 0x09, 0x00, 0xff, 0x87, 0x6c, 0x2d, 0xaf, 0x73,
      0x79, 0xc8, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
      0x01, 0x01, 0x0b, 0x05, 0x00, 0x30, 0x2e, 0x31, 0x2c, 0x30, 0x2a, 0x06,
      0x03, 0x55, 0x04, 0x03, 0x13, 0x23, 0x59, 0x75, 0x62, 0x69, 0x63, 0x6f,
      0x20, 0x55, 0x32, 0x46, 0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41,
      0x20, 0x53, 0x65, 0x72, 0x69, 0x61, 0x6c, 0x20, 0x34, 0x35, 0x37, 0x32,
      0x30, 0x30, 0x36, 0x33, 0x31, 0x30, 0x20, 0x17, 0x0d, 0x31, 0x34, 0x30,
      0x38, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f,
      0x32, 0x30, 0x35, 0x30, 0x30, 0x39, 0x30, 0x34, 0x30, 0x30, 0x30, 0x30,
      0x30, 0x30, 0x5a, 0x30, 0x6e, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55,
      0x04, 0x06, 0x13, 0x02, 0x53, 0x45, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03,
      0x55, 0x04, 0x0a, 0x0c, 0x09, 0x59, 0x75, 0x62, 0x69, 0x63, 0x6f, 0x20,
      0x41, 0x42, 0x31, 0x22, 0x30, 0x20, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x0c,
      0x19, 0x41, 0x75, 0x74, 0x68, 0x65, 0x6e, 0x74, 0x69, 0x63, 0x61, 0x74,
      0x6f, 0x72, 0x20, 0x41, 0x74, 0x74, 0x65, 0x73, 0x74, 0x61, 0x74, 0x69,
      0x6f, 0x6e, 0x31, 0x27, 0x30, 0x25, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c,
      0x1e, 0x59, 0x75, 0x62, 0x69, 0x63, 0x6f, 0x20, 0x55, 0x32, 0x46, 0x20,
      0x45, 0x45, 0x20, 0x53, 0x65, 0x72, 0x69, 0x61, 0x6c, 0x20, 0x37, 0x36,
      0x32, 0x30, 0x38, 0x37, 0x34, 0x32, 0x33, 0x30, 0x59, 0x30, 0x13, 0x06,
      0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86,
      0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0x25, 0xf1,
      0x23, 0xa0, 0x48, 0x28, 0x3f, 0xc5, 0x79, 0x6c, 0xcf, 0x88, 0x7d, 0x99,
      0x48, 0x9f, 0xd9, 0x35, 0xc2, 0x41, 0x98, 0xc4, 0xb5, 0xd8, 0xd5, 0xb2,
      0xc2, 0xbf, 0xd7, 0xdd, 0x5d, 0x15, 0xaf, 0xe4, 0x5b, 0x70, 0x70, 0x77,
      0x65, 0x67, 0xd5, 0xb5, 0xb0, 0xb2, 0x3e, 0x04, 0x56, 0x0b, 0x5b, 0xea,
      0x77, 0xb4, 0x83, 0xb1, 0xf6, 0x49, 0x1e, 0x53, 0xa3, 0xf2, 0xbe, 0xe6,
      0xa3, 0x9a, 0xa3, 0x81, 0x81, 0x30, 0x7f, 0x30, 0x13, 0x06, 0x0a, 0x2b,
      0x06, 0x01, 0x04, 0x01, 0x82, 0xc4, 0x0a, 0x0d, 0x01, 0x04, 0x05, 0x04,
      0x03, 0x05, 0x05, 0x06, 0x30, 0x22, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x04,
      0x01, 0x82, 0xc4, 0x0a, 0x02, 0x04, 0x15, 0x31, 0x2e, 0x33, 0x2e, 0x36,
      0x2e, 0x31, 0x2e, 0x34, 0x2e, 0x31, 0x2e, 0x34, 0x31, 0x34, 0x38, 0x32,
      0x2e, 0x31, 0x2e, 0x39, 0x30, 0x13, 0x06, 0x0b, 0x2b, 0x06, 0x01, 0x04,
      0x01, 0x82, 0xe5, 0x1c, 0x02, 0x01, 0x01, 0x04, 0x04, 0x03, 0x02, 0x05,
      0x20, 0x30, 0x21, 0x06, 0x0b, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0xe5,
      0x1c, 0x01, 0x01, 0x04, 0x04, 0x12, 0x04, 0x10, 0xd8, 0x52, 0x2d, 0x9f,
      0x57, 0x5b, 0x48, 0x66, 0x88, 0xa9, 0xba, 0x99, 0xfa, 0x02, 0xf3, 0x5b,
      0x30, 0x0c, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x02,
      0x30, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
      0x01, 0x01, 0x0b, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00, 0x52, 0xb0,
      0x69, 0x49, 0xdb, 0xaa, 0xd1, 0xa6, 0x4c, 0x1b, 0xa9, 0xeb, 0xc1, 0x98,
      0xb3, 0x17, 0xec, 0x31, 0xf9, 0xa3, 0x73, 0x63, 0xba, 0x51, 0x61, 0xb3,
      0x42, 0xe3, 0xa4, 0x9c, 0xad, 0x50, 0x4f, 0x34, 0xe7, 0x42, 0x8b, 0xb8,
      0x96, 0xe9, 0xcf, 0xd2, 0x8d, 0x03, 0xad, 0x10, 0xce, 0x32, 0x5a, 0x06,
      0x83, 0x8e, 0x9b, 0x6c, 0x4e, 0xcb, 0x17, 0xad, 0x40, 0xd0, 0x90, 0xa1,
      0x6c, 0x9e, 0x7c, 0x34, 0x49, 0x83, 0x32, 0xff, 0x85, 0x3b, 0x62, 0x74,
      0x7e, 0x8f, 0xcd, 0xf0, 0x0d, 0xae, 0x62, 0x75, 0x6e, 0x57, 0xbd, 0x40,
      0xb1, 0x6d, 0x67, 0x79, 0x07, 0xa8, 0x35, 0xc0, 0x43, 0x5a, 0x2e, 0xbc,
      0xe9, 0xb0, 0xb9, 0x06, 0x9c, 0xa1, 0x22, 0xbf, 0x9d, 0x96, 0x4a, 0x73,
      0x20, 0x6a, 0xf7, 0x4f, 0xf3, 0xc0, 0x01, 0x44, 0xeb, 0xff, 0x3d, 0xe7,
      0xc7, 0x75, 0x8d, 0x31, 0x47, 0xc8, 0xc2, 0xf9, 0xfe, 0x87, 0xc1, 0x2f,
      0x2a, 0x96, 0x75, 0xa2, 0x04, 0x6b, 0x01, 0x07, 0x63, 0x61, 0xa9, 0x97,
      0x21, 0x87, 0x1f, 0xa7, 0x8f, 0xb0, 0xde, 0x29, 0x45, 0xb5, 0x79, 0xf9,
      0x16, 0x6c, 0x48, 0xad, 0x2f, 0xd5, 0x0c, 0x3c, 0xe5, 0x6c, 0x82, 0x21,
      0xa7, 0x50, 0x83, 0xf6, 0x56, 0x11, 0x93, 0x94, 0x36, 0x8f, 0xf1, 0x7d,
      0x2c, 0x92, 0x0c, 0x63, 0xa0, 0x9f, 0x01, 0xed, 0x25, 0x01, 0x14, 0x6b,
      0x7d, 0xf1, 0xab, 0x39, 0x70, 0xa2, 0xa3, 0x29, 0x38, 0xfa, 0x9a, 0x51,
      0x7a, 0xf4, 0x71, 0x08, 0x5e, 0x16, 0x0b, 0x3c, 0xa7, 0x97, 0x64, 0x23,
      0x17, 0x46, 0xba, 0x6a, 0xbb, 0xa6, 0x8e, 0x0d, 0x13, 0xce, 0x25, 0x97,
      0x96, 0xbc, 0xd2, 0xa0, 0x3a, 0xd8, 0x3c, 0x74, 0xe1, 0x53, 0x31, 0x32,
      0x8e, 0xab, 0x43, 0x8e, 0x6a, 0x41, 0x97, 0xcb, 0x12, 0xec, 0x6f, 0xd1,
      0xe3, 0x88, 0x68, 0x61, 0x75, 0x74, 0x68, 0x44, 0x61, 0x74, 0x61, 0x58,
      0xc4, 0x26, 0xbd, 0x72, 0x78, 0xbe, 0x46, 0x37, 0x61, 0xf1, 0xfa, 0xa1,
      0xb1, 0x0a, 0xb4, 0xc4, 0xf8, 0x26, 0x70, 0x26, 0x9c, 0x41, 0x0c, 0x72,
      0x6a, 0x1f, 0xd6, 0xe0, 0x58, 0x55, 0xe1, 0x9b, 0x46, 0x45, 0x00, 0x00,
      0x00, 0x03, 0xd8, 0x52, 0x2d, 0x9f, 0x57, 0x5b, 0x48, 0x66, 0x88, 0xa9,
      0xba, 0x99, 0xfa, 0x02, 0xf3, 0x5b, 0x00, 0x40, 0x91, 0x88, 0xee, 0xf6,
      0xe9, 0x75, 0xef, 0x4e, 0x8b, 0x5b, 0x91, 0x34, 0xbf, 0x59, 0x89, 0x37,
      0xe7, 0x91, 0x60, 0x21, 0xeb, 0x61, 0x5d, 0x23, 0x83, 0xe4, 0x33, 0xe9,
      0xbc, 0x59, 0xb4, 0x7e, 0xf0, 0xae, 0xfb, 0x4d, 0xad, 0xb5, 0xde, 0x9e,
      0x0c, 0x41, 0x00, 0x5b, 0xdc, 0xc0, 0x14, 0xb2, 0x18, 0x16, 0xf1, 0xfb,
      0x8d, 0xe7, 0x67, 0x69, 0x71, 0xb3, 0x4e, 0xd3, 0x27, 0xfe, 0x7a, 0x4c,
      0xa5, 0x01, 0x02, 0x03, 0x26, 0x20, 0x01, 0x21, 0x58, 0x20, 0xa2, 0x68,
      0x2f, 0x95, 0x1e, 0xdf, 0x6c, 0xba, 0xe7, 0x20, 0xc9, 0x74, 0xe2, 0x3a,
      0xb5, 0xeb, 0x1a, 0x0d, 0xdf, 0xf9, 0x1a, 0xf0, 0x80, 0x41, 0x40, 0x28,
      0x8f, 0xaf, 0x34, 0x58, 0xe4, 0xc5, 0x22, 0x58, 0x20, 0x32, 0x91, 0xcc,
      0x36, 0xcb, 0xa9, 0xe7, 0xf6, 0x4b, 0xaf, 0xf9, 0xbc, 0x84, 0x1d, 0x1a,
      0x66, 0xc8, 0x01, 0x1c, 0x05, 0x42, 0x31, 0x3a, 0x26, 0x3a, 0x5d, 0x2a,
      0x12, 0xd6, 0x6d, 0x26, 0xf4};
  static const std::vector<uint8_t> kPublicKeyBytes = {
      0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
      0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
      0x42, 0x00, 0x04, 0xa2, 0x68, 0x2f, 0x95, 0x1e, 0xdf, 0x6c, 0xba, 0xe7,
      0x20, 0xc9, 0x74, 0xe2, 0x3a, 0xb5, 0xeb, 0x1a, 0x0d, 0xdf, 0xf9, 0x1a,
      0xf0, 0x80, 0x41, 0x40, 0x28, 0x8f, 0xaf, 0x34, 0x58, 0xe4, 0xc5, 0x32,
      0x91, 0xcc, 0x36, 0xcb, 0xa9, 0xe7, 0xf6, 0x4b, 0xaf, 0xf9, 0xbc, 0x84,
      0x1d, 0x1a, 0x66, 0xc8, 0x01, 0x1c, 0x05, 0x42, 0x31, 0x3a, 0x26, 0x3a,
      0x5d, 0x2a, 0x12, 0xd6, 0x6d, 0x26, 0xf4,
  };

  {
    const std::optional<AttestationObject::ResponseFields> fields =
        AttestationObject::ParseForResponseFields(
            kAttestationObjectBytes, /*attestation_acceptable=*/true);
    ASSERT_TRUE(fields);

    const AttestationObject attestation_object =
        *AttestationObject::Parse(*cbor::Reader::Read(kAttestationObjectBytes));
    EXPECT_EQ(fields->attestation_object_bytes, kAttestationObjectBytes);
    EXPECT_EQ(fields->authenticator_data,
              attestation_object.authenticator_data().SerializeToByteArray());
    EXPECT_EQ(fields->public_key_algo, -7);
    EXPECT_EQ(fields->public_key_der, kPublicKeyBytes);
  }

  {
    const std::optional<AttestationObject::ResponseFields> fields =
        AttestationObject::ParseForResponseFields(
            kAttestationObjectBytes, /*attestation_acceptable=*/false);
    ASSERT_TRUE(fields);

    EXPECT_NE(fields->attestation_object_bytes, kAttestationObjectBytes);
    EXPECT_EQ(fields->public_key_algo, -7);
    EXPECT_EQ(fields->public_key_der, kPublicKeyBytes);

    const AttestationObject attestation_object = *AttestationObject::Parse(
        *cbor::Reader::Read(fields->attestation_object_bytes));
    EXPECT_EQ(fields->authenticator_data,
              attestation_object.authenticator_data().SerializeToByteArray());
    EXPECT_FALSE(attestation_object.authenticator_data()
                     .attested_data()
                     ->IsAaguidZero());
    EXPECT_TRUE(attestation_object.attestation_statement().IsNoneAttestation());
  }
}

}  // namespace device
