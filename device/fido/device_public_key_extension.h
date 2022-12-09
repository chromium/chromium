// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_DEVICE_PUBLIC_KEY_EXTENSION_H_
#define DEVICE_FIDO_DEVICE_PUBLIC_KEY_EXTENSION_H_

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "components/cbor/values.h"
#include "device/fido/fido_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

// DevicePublicKeyRequest reflects the RP-provided `devicePubKey` extension in
// a create() or get() request. See https://github.com/w3c/webauthn/pull/1663.
struct COMPONENT_EXPORT(DEVICE_FIDO) DevicePublicKeyRequest {
  DevicePublicKeyRequest();
  DevicePublicKeyRequest(DevicePublicKeyRequest&&);
  DevicePublicKeyRequest(const DevicePublicKeyRequest&);
  ~DevicePublicKeyRequest();
  DevicePublicKeyRequest& operator=(const DevicePublicKeyRequest&);
  bool operator==(const DevicePublicKeyRequest&) const;

  // FromCBOR parses a `DevicePublicKeyRequest` from the given CBOR map, which
  // should be the extension contents. The `ep_approved_by_browser` argument
  // controls whether "enterprise" attestation is interpreted as "only if the
  // authenticator recognises the RP ID" or whether the browser has approved it
  // for all cases.
  static absl::optional<DevicePublicKeyRequest> FromCBOR(
      const cbor::Value& map,
      bool ep_approved_by_browser);

  cbor::Value ToCBOR() const;

  AttestationConveyancePreference attestation =
      AttestationConveyancePreference::kNone;
  std::vector<std::string> attestation_formats;
};

// DevicePublicKeyOutput reflects the output of the `devicePubKey` extension in
// a create() or get() call. The contents of the CBOR bytestring are parsed and
// expanded into this structure.
struct COMPONENT_EXPORT(DEVICE_FIDO) DevicePublicKeyOutput {
  DevicePublicKeyOutput();
  DevicePublicKeyOutput(DevicePublicKeyOutput&&);
  ~DevicePublicKeyOutput();

  static absl::optional<DevicePublicKeyOutput> FromExtension(
      const cbor::Value& value);

  std::array<uint8_t, 16> aaguid = {0};
  cbor::Value dpk;
  unsigned scope = 0;
  std::vector<uint8_t> nonce;
  std::string attestation_format;
  cbor::Value attestation;
  bool enterprise_attestation_returned = false;
};

// CheckDevicePublicKeyExtensionForErrors checks that `extension_value` is a
// sensible extension response given that `requested_attestation` was the DPK
// attestation level in the request and `backup_eligible_flag` is the value of
// the BE flag from the authenticator data. It returns an error message on
// failure or `absl::nullopt` on success.
absl::optional<const char*> CheckDevicePublicKeyExtensionForErrors(
    const cbor::Value& extension_value,
    AttestationConveyancePreference requested_attestation,
    bool backup_eligible_flag);

}  // namespace device

#endif  // DEVICE_FIDO_DEVICE_PUBLIC_KEY_EXTENSION_H_
