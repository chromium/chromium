// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains additional declarations for CTAP2 PIN support. Only
// implementations of the PIN protocol should need to include this file. For all
// other code, see |pin.h|.

#ifndef DEVICE_FIDO_PIN_INTERNAL_H_
#define DEVICE_FIDO_PIN_INTERNAL_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace device {
namespace pin {

// Subcommand enumerates the subcommands to the main |authenticatorClientPIN|
// command. See
// https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#authenticatorClientPIN
enum class Subcommand : uint8_t {
  kGetRetries = 0x01,
  kGetKeyAgreement = 0x02,
  kSetPIN = 0x03,
  kChangePIN = 0x04,
  kGetPINToken = 0x05,
  kGetUvToken = 0x06,
  kGetUvRetries = 0x07,
  kSetMinPINLength = 0x08,
  kGetPinUvAuthTokenUsingPinWithPermissions = 0x09,
};

// RequestKey enumerates the keys in the top-level CBOR map for all PIN
// commands.
enum class RequestKey : int {
  kProtocol = 0x01,
  kSubcommand = 0x02,
  kKeyAgreement = 0x03,
  kPINAuth = 0x04,
  kNewPINEnc = 0x05,
  kPINHashEnc = 0x06,
  kMinPINLength = 0x07,
  kMinPINLengthRPIDs = 0x08,
  kPermissions = 0x09,
  kPermissionsRPID = 0x0A,
};

// ResponseKey enumerates the keys in the top-level CBOR map for all PIN
// responses.
enum class ResponseKey : int {
  kKeyAgreement = 1,
  kPINToken = 2,
  kRetries = 3,
  kUvRetries = 5,
};

// PointFromKeyAgreementResponse returns an |EC_POINT| that represents the same
// P-256 point as |response|. It returns |nullopt| if |response| encodes an
// invalid point.
std::optional<bssl::UniquePtr<EC_POINT>> PointFromKeyAgreementResponse(
    const EC_GROUP* group,
    const KeyAgreementResponse& response);

// Protocol abstracts a PIN/UV Auth Token Protocol. Instances are obtained
// through ProtocolVersion().
class COMPONENT_EXPORT(DEVICE_FIDO) Protocol {
 public:
  virtual ~Protocol() = default;
  Protocol(Protocol&) = delete;
  Protocol& operator=(Protocol&) = delete;

  // Encapsulate derives a shared secret, which it writes to |out_shared_key|,
  // and returns the platform's public key.
  virtual std::array<uint8_t, kP256X962Length> Encapsulate(
      const KeyAgreementResponse& peers_key,
      std::vector<uint8_t>* out_shared_key) const = 0;

  // Encrypt encrypts |plaintext| under |shared_key|. |plaintext| must be a
  // multiple of AES_BLOCK_SIZE in length. |shared_key| must be a shared secret
  // derived by Encapsulate().
  virtual std::vector<uint8_t> Encrypt(
      base::span<const uint8_t> shared_key,
      base::span<const uint8_t> plaintext) const = 0;

  // Decrypt returns the decryption of |ciphertext|, which must be non-empty and
  // a multiple of AES_BLOCK_SIZE in length. |shared_key| must be a shared
  // secret derived by Encapsulate().
  virtual std::vector<uint8_t> Decrypt(
      base::span<const uint8_t> shared_key,
      base::span<const uint8_t> ciphertext) const = 0;

  // Authenticate() returns a signature of |data|. |key| must be either a shared
  // secret derived by Encapsulate() or a valid pinUvAuthToken from the same
  // PIN/UV Auth Protocol.
  virtual std::vector<uint8_t> Authenticate(
      base::span<const uint8_t> key,
      base::span<const uint8_t> data) const = 0;

  // Verify verifies a signature computed by Authenticate().
  virtual bool Verify(base::span<const uint8_t> key,
                      base::span<const uint8_t> data,
                      base::span<const uint8_t> signature) const = 0;

  // CalculateSharedKey returns the CTAP2 shared key between |key| and
  // |peers_key|.
  virtual std::vector<uint8_t> CalculateSharedKey(
      const EC_KEY* key,
      const EC_POINT* peers_key) const = 0;

 protected:
  Protocol() = default;
};

// ProtocolVersion returns the |Protocol| implementation for
// |PINUvAuthProtocol|.
COMPONENT_EXPORT(DEVICE_FIDO)
const Protocol& ProtocolVersion(PINUVAuthProtocol protocol);

}  // namespace pin
}  // namespace device

#endif  // DEVICE_FIDO_PIN_INTERNAL_H_
