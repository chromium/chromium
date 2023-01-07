// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_GET_INFO_RESPONSE_H_
#define DEVICE_FIDO_AUTHENTICATOR_GET_INFO_RESPONSE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  absl::optional<uint32_t> max_msg_size;
  absl::optional<uint32_t> max_credential_count_in_list;
  absl::optional<uint32_t> max_credential_id_length;
  absl::optional<base::flat_set<PINUVAuthProtocol>> pin_protocols;
  absl::optional<std::vector<std::string>> extensions;
  absl::optional<std::vector<int32_t>> algorithms;
  absl::optional<uint32_t> max_serialized_large_blob_array;
  absl::optional<uint32_t> remaining_discoverable_credentials;
  absl::optional<bool> force_pin_change;
  absl::optional<uint32_t> min_pin_length;
  absl::optional<base::flat_set<FidoTransportProtocol>> transports;

  // max_cred_blob_length is the maximum size credBlob that the authenticator
  // supports per credential, or nullopt if credBlob is not supported. If
  // present, this value will be >= 32.
  absl::optional<uint32_t> max_cred_blob_length;

  AuthenticatorSupportedOptions options;
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_GET_INFO_RESPONSE_H_
