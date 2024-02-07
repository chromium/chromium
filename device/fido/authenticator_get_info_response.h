// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_GET_INFO_RESPONSE_H_
#define DEVICE_FIDO_AUTHENTICATOR_GET_INFO_RESPONSE_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"

namespace device {

// Authenticator response for AuthenticatorGetInfo request that encapsulates
// versions, options, AAGUID(Authenticator Attestation GUID), other
// authenticator device information.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticatorGetInfo
struct COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorGetInfoResponse {
 public:
  AuthenticatorGetInfoResponse(base::flat_set<ProtocolVersion> versions,
                               base::flat_set<Ctap2Version> in_ctap2_version,
                               base::span<const uint8_t, kAaguidLength> aaguid);
  AuthenticatorGetInfoResponse(AuthenticatorGetInfoResponse&& that);
  AuthenticatorGetInfoResponse& operator=(AuthenticatorGetInfoResponse&& other);

  AuthenticatorGetInfoResponse(const AuthenticatorGetInfoResponse&) = delete;
  AuthenticatorGetInfoResponse& operator=(const AuthenticatorGetInfoResponse&) =
      delete;

  ~AuthenticatorGetInfoResponse();

  static std::vector<uint8_t> EncodeToCBOR(
      const AuthenticatorGetInfoResponse& response);

  // Returns true if there is a Ctap2Version in |ctap2_versions| greater or
  // equal to |ctap2_version|.
  bool SupportsAtLeast(Ctap2Version ctap2_version) const;

  base::flat_set<ProtocolVersion> versions;
  base::flat_set<Ctap2Version> ctap2_versions;
  std::array<uint8_t, kAaguidLength> aaguid;
  std::optional<uint32_t> max_msg_size;
  std::optional<uint32_t> max_credential_count_in_list;
  std::optional<uint32_t> max_credential_id_length;
  std::optional<base::flat_set<PINUVAuthProtocol>> pin_protocols;
  std::optional<std::vector<std::string>> extensions;
  std::optional<std::vector<int32_t>> algorithms;
  std::optional<uint32_t> max_serialized_large_blob_array;
  std::optional<uint32_t> remaining_discoverable_credentials;
  std::optional<bool> force_pin_change;
  std::optional<uint32_t> min_pin_length;
  std::optional<base::flat_set<FidoTransportProtocol>> transports;
  AuthenticatorSupportedOptions options;
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_GET_INFO_RESPONSE_H_
