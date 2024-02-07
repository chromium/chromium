// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/win/type_conversions.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/attestation_object.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

TEST(TypeConversionsTest, ToAuthenticatorMakeCredentialResponse) {
  struct TestCase {
    const wchar_t* format;
    std::vector<uint8_t> authenticator_data;
    std::vector<uint8_t> cbor_attestation_statement;
    uint8_t used_transport;  // WEBAUTHN_CTAP_TRANSPORT_* from <webauthn.h>
    bool success;
    std::optional<FidoTransportProtocol> expected_transport;
  } test_cases[] = {
      {L"packed",
       fido_parsing_utils::Materialize(test_data::kTestSignAuthenticatorData),
       fido_parsing_utils::Materialize(
           test_data::kPackedAttestationStatementCBOR),
       WEBAUTHN_CTAP_TRANSPORT_USB, true,
       FidoTransportProtocol::kUsbHumanInterfaceDevice},
      {L"packed",
       fido_parsing_utils::Materialize(test_data::kTestSignAuthenticatorData),
       fido_parsing_utils::Materialize(
           test_data::kPackedAttestationStatementCBOR),
       WEBAUTHN_CTAP_TRANSPORT_NFC, true,
       FidoTransportProtocol::kNearFieldCommunication},
      {L"packed",
       fido_parsing_utils::Materialize(test_data::kTestSignAuthenticatorData),
       fido_parsing_utils::Materialize(
           test_data::kPackedAttestationStatementCBOR),
       WEBAUTHN_CTAP_TRANSPORT_INTERNAL, true,
       FidoTransportProtocol::kInternal},
      {L"packed",
       fido_parsing_utils::Materialize(test_data::kTestSignAuthenticatorData),
       fido_parsing_utils::Materialize(
           test_data::kPackedAttestationStatementCBOR),
       WEBAUTHN_CTAP_TRANSPORT_TEST, true, std::nullopt},
      // Unknown attestation formats
      {L"weird-unknown-format",
       fido_parsing_utils::Materialize(test_data::kTestSignAuthenticatorData),
       {0xa0},  // Empty CBOR map.
       WEBAUTHN_CTAP_TRANSPORT_USB,
       true,
       FidoTransportProtocol::kUsbHumanInterfaceDevice},
      {L"weird-unknown-format",
       fido_parsing_utils::Materialize(test_data::kTestSignAuthenticatorData),
       {0x60},  // Empty string. Not a valid attStmt.
       WEBAUTHN_CTAP_TRANSPORT_USB,
       false},
      // Invalid authenticator data
      {L"packed",
       {},
       fido_parsing_utils::Materialize(
           test_data::kPackedAttestationStatementCBOR),
       WEBAUTHN_CTAP_TRANSPORT_USB,
       false},
      {L"packed",
       {1, 2, 3},
       fido_parsing_utils::Materialize(
           test_data::kPackedAttestationStatementCBOR),
       WEBAUTHN_CTAP_TRANSPORT_USB,
       false},
      // Invalid attestation statement
      {L"packed",
       fido_parsing_utils::Materialize(test_data::kTestSignAuthenticatorData),
       {},
       WEBAUTHN_CTAP_TRANSPORT_USB,
       false},
      {L"packed",
       fido_parsing_utils::Materialize(test_data::kTestSignAuthenticatorData),
       {1, 2, 3},
       WEBAUTHN_CTAP_TRANSPORT_USB,
       false},
  };
  size_t i = 0;
  for (const auto& test : test_cases) {
    SCOPED_TRACE(::testing::Message() << "Test case " << i++);
    auto response =
        ToAuthenticatorMakeCredentialResponse(WEBAUTHN_CREDENTIAL_ATTESTATION{
            WEBAUTHN_CREDENTIAL_ATTESTATION_VERSION_3,
            test.format,
            base::checked_cast<DWORD>(test.authenticator_data.size()),
            const_cast<unsigned char*>(test.authenticator_data.data()),
            base::checked_cast<DWORD>(test.cbor_attestation_statement.size()),
            const_cast<unsigned char*>(test.cbor_attestation_statement.data()),
            // dwAttestationDecodeType and pvAttestationDecode are ignored.
            WEBAUTHN_ATTESTATION_DECODE_NONE,
            nullptr,
            // cbAttestationObject and pbAttestationObject are ignored.
            0,
            nullptr,
            // cbCredentialId and pbCredentialId are ignored.
            0,
            nullptr,
            WEBAUTHN_EXTENSIONS{},
            test.used_transport,
        });
    EXPECT_EQ(response.has_value(), test.success);
    if (!response)
      return;

    EXPECT_EQ(response->attestation_object.authenticator_data()
                  .SerializeToByteArray(),
              test.authenticator_data);
    EXPECT_EQ(
        response->attestation_object.attestation_statement().format_name(),
        base::WideToUTF8(test.format));
    EXPECT_EQ(cbor::Writer::Write(
                  AsCBOR(response->attestation_object.attestation_statement())),
              test.cbor_attestation_statement);
    EXPECT_EQ(response->transport_used, test.expected_transport);
    if (test.expected_transport == FidoTransportProtocol::kInternal) {
      EXPECT_THAT(*response->transports,
                  testing::ElementsAre(FidoTransportProtocol::kInternal));
    } else {
      EXPECT_FALSE(response->transports);
    }
  }
}

TEST(TypeConversionsTest, Transports) {
  for (int i = 0; i < 16; i++) {
    const uint32_t mask = 1u << i;
    const std::optional<FidoTransportProtocol> transport =
        FromWinTransportsMask(mask);
    if (transport) {
      const uint32_t result = ToWinTransportsMask({*transport});
      EXPECT_EQ(result, mask);
    }
  }
}

}  // namespace
}  // namespace device
