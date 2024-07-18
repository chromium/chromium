// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "components/cbor/reader.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_get_info_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap2_device_operation.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/get_assertion_task.h"

namespace device {

// Creating a PublicKeyCredentialUserEntity from a CBOR value can involve URL
// parsing, which relies on ICU for IDN handling. This is why ICU needs to be
// initialized explicitly.
// See: http://crbug/808412
struct IcuEnvironment {
  IcuEnvironment() { CHECK(base::i18n::InitializeICU()); }
  // Used by ICU integration.
  base::AtExitManager at_exit_manager;
};

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  cbor::Reader::Config config;
  config.allow_invalid_utf8 = true;
  std::vector<uint8_t> input(data, data + size);
  std::optional<cbor::Value> input_cbor = cbor::Reader::Read(input, config);
  if (input_cbor) {
    input_cbor =
        FixInvalidUTF8(std::move(*input_cbor),
                       [](const std::vector<const cbor::Value*>& path) {
                         return path.size() == 2;
                       });
  }

  std::array<uint8_t, 32> relying_party_id_hash = {};
  auto response = ReadCTAPMakeCredentialResponse(
      FidoTransportProtocol::kUsbHumanInterfaceDevice, input_cbor);
  if (response) {
    response->attestation_object.EraseAttestationStatement(
        AttestationObject::AAGUID::kErase);
  }

  response = AuthenticatorMakeCredentialResponse::CreateFromU2fRegisterResponse(
      FidoTransportProtocol::kUsbHumanInterfaceDevice, relying_party_id_hash,
      input);
  if (response) {
    response->attestation_object.EraseAttestationStatement(
        AttestationObject::AAGUID::kErase);
  }

  ReadCTAPGetAssertionResponse(FidoTransportProtocol::kUsbHumanInterfaceDevice,
                               input_cbor);
  std::vector<uint8_t> u2f_response_data(data, data + size);
  std::vector<uint8_t> key_handle(data, data + size);
  AuthenticatorGetAssertionResponse::CreateFromU2fSignResponse(
      relying_party_id_hash, u2f_response_data, key_handle,
      FidoTransportProtocol::kUsbHumanInterfaceDevice);

  ReadCTAPGetInfoResponse(input);
  return 0;
}

}  // namespace device
