// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_V2_HANDSHAKE_H_
#define DEVICE_FIDO_CABLE_V2_HANDSHAKE_H_

#include <stdint.h>

#include <array>
#include <memory>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "components/cbor/values.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/noise.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/fido_constants.h"
#include "third_party/boringssl/src/include/openssl/base.h"

class GURL;

namespace device {
namespace cablev2 {

namespace tunnelserver {

// DecodeDomain converts a 22-bit tunnel server domain (as encoded by
// |EncodeDomain|) into a string in dotted form.
COMPONENT_EXPORT(DEVICE_FIDO) std::string DecodeDomain(uint16_t domain);

// GetNewTunnelURL converts a 22-bit tunnel server domain (as encoded by
// |EncodeDomain|), and a tunnel ID, into a WebSockets-based URL for creating a
// new tunnel.
COMPONENT_EXPORT(DEVICE_FIDO)
GURL GetNewTunnelURL(uint16_t domain, base::span<const uint8_t, 16> id);

// GetConnectURL converts a 22-bit tunnel server domain (as encoded by
// |EncodeDomain|), a routing-ID, and a tunnel ID, into a WebSockets-based URL
// for connecting to an existing tunnel.
COMPONENT_EXPORT(DEVICE_FIDO)
GURL GetConnectURL(uint16_t domain,
                   std::array<uint8_t, kRoutingIdSize> routing_id,
                   base::span<const uint8_t, 16> id);

// GetContactURL gets a URL for contacting a previously-paired authenticator.
// The |tunnel_server| is assumed to be a valid domain name and should have been
// taken from a previous call to |DecodeDomain|.
COMPONENT_EXPORT(DEVICE_FIDO)
GURL GetContactURL(const std::string& tunnel_server,
                   base::span<const uint8_t> contact_id);

}  // namespace tunnelserver

namespace eid {

// Encrypt turns an EID into a BLE advert payload by encrypting and
// authenticating with |key|.
COMPONENT_EXPORT(DEVICE_FIDO)
std::array<uint8_t, kAdvertSize> Encrypt(
    const CableEidArray& eid,
    base::span<const uint8_t, kEIDKeySize> key);

// Decrypt turns a BLE advert payload into a plaintext EID (suitable for passing
// to |FromComponents|) by decrypting with |key|.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<CableEidArray> Decrypt(
    const std::array<uint8_t, kAdvertSize>& advert,
    base::span<const uint8_t, kEIDKeySize> key);

// TODO(agl): this could probably be a class.

// Components contains the parts of a decrypted EID.
struct Components {
  uint16_t tunnel_server_domain;
  std::array<uint8_t, kRoutingIdSize> routing_id;
  std::array<uint8_t, kNonceSize> nonce;
};

// FromComponents constructs a valid EID from the given components. |IsValid|
// will be true of the result.
COMPONENT_EXPORT(DEVICE_FIDO)
CableEidArray FromComponents(const Components& components);

// ToComponents explodes a decrypted EID into its components. It's the
// inverse of |ComponentsToEID|. |IsValid| must be true for the given EID before
// calling this function.
COMPONENT_EXPORT(DEVICE_FIDO)
Components ToComponents(const CableEidArray& eid);

}  // namespace eid

namespace qr {

// Components contains the parsed elements of a QR code.
struct COMPONENT_EXPORT(DEVICE_FIDO) Components {
  std::array<uint8_t, device::kP256X962Length> peer_identity;
  std::array<uint8_t, 16> secret;
};

COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<Components> Parse(const std::string& qr_url);

// Encode returns the contents of a QR code that represents |qr_key|.
COMPONENT_EXPORT(DEVICE_FIDO)
std::string Encode(base::span<const uint8_t, kQRKeySize> qr_key);

}  // namespace qr

// DerivedValueType enumerates the different types of values that might be
// derived in caBLEv2 from some secret. The values this this enum are protocol
// constants and thus must not change over time.
enum class DerivedValueType : uint32_t {
  kEIDKey = 1,
  kTunnelID = 2,
  kPSK = 3,
  kPairedSecret = 4,
  kIdentityKeySeed = 5,
};

namespace internal {
COMPONENT_EXPORT(DEVICE_FIDO)
void Derive(uint8_t* out,
            size_t out_len,
            base::span<const uint8_t> secret,
            base::span<const uint8_t> nonce,
            DerivedValueType type);
}  // namespace internal

// Derive derives a sub-secret from a secret and nonce. It is not possible to
// learn anything about |secret| from the value of the sub-secret, assuming that
// |secret| has sufficient size to prevent full enumeration of the
// possibilities.
template <size_t N>
std::array<uint8_t, N> Derive(base::span<const uint8_t> secret,
                              base::span<const uint8_t> nonce,
                              DerivedValueType type) {
  std::array<uint8_t, N> ret;
  internal::Derive(ret.data(), N, secret, nonce, type);
  return ret;
}

// EncodePaddedCBORMap encodes the given map and pads it to 256 bytes in such a
// way that |DecodePaddedCBORMap| can decode it. The padding is done on the
// assumption that the returned bytes will be encrypted and the encoded size of
// the map should be hidden. The function can fail if the CBOR encoding fails
// or, somehow, the size overflows.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<std::vector<uint8_t>> EncodePaddedCBORMap(
    cbor::Value::MapValue map);

// DecodePaddedCBORMap unpads and decodes a CBOR map as produced by
// |EncodePaddedCBORMap|.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<cbor::Value> DecodePaddedCBORMap(
    base::span<const uint8_t> input);

// Crypter handles the post-handshake encryption of CTAP2 messages.
class COMPONENT_EXPORT(DEVICE_FIDO) Crypter {
 public:
  Crypter(base::span<const uint8_t, 32> read_key,
          base::span<const uint8_t, 32> write_key);
  ~Crypter();

  // Encrypt encrypts |message_to_encrypt| and overrides it with the
  // ciphertext. It returns true on success and false on error.
  bool Encrypt(std::vector<uint8_t>* message_to_encrypt);

  // Decrypt decrypts |ciphertext|, which was received as the payload of a
  // message with the given command, and writes the plaintext to
  // |out_plaintext|. It returns true on success and false on error.
  //
  // (In practice, command must always be |kMsg|. But passing it here makes it
  // less likely that other code will forget to check that.)
  bool Decrypt(base::span<const uint8_t> ciphertext,
               std::vector<uint8_t>* out_plaintext);

  // IsCounterpartyOfForTesting returns true if |other| is the mirror-image of
  // this object. (I.e. read/write keys are equal but swapped.)
  bool IsCounterpartyOfForTesting(const Crypter& other) const;

 private:
  const std::array<uint8_t, 32> read_key_, write_key_;
  uint32_t read_sequence_num_ = 0;
  uint32_t write_sequence_num_ = 0;
};

// HandshakeHash is the hashed transcript of a handshake. This can be used as a
// channel-binding value. See
// http://www.noiseprotocol.org/noise.html#channel-binding.
using HandshakeHash = std::array<uint8_t, 32>;

// HandshakeResult is the output of the handshaking process on both sides: a
// |Crypter| that can encrypt and decrypt future messages on the connection, and
// the handshake hash that can be used to tie signatures to the connection.
using HandshakeResult =
    base::Optional<std::pair<std::unique_ptr<Crypter>, HandshakeHash>>;

// HandshakeInitiator starts a caBLE v2 handshake and processes the single
// response message from the other party. The handshake is always initiated from
// the desktop.
class COMPONENT_EXPORT(DEVICE_FIDO) HandshakeInitiator {
 public:
  HandshakeInitiator(
      // psk is derived from the connection nonce and either QR-code secrets
      // pairing secrets.
      base::span<const uint8_t, 32> psk,
      // peer_identity, if not nullopt, specifies that this is a paired
      // handshake and then contains a P-256 public key for the peer. Otherwise
      // this is a QR handshake.
      base::Optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
      // identity_seed, if not nullopt, specifies that this is a QR handshake
      // and contains the seed for QR key for this client. identity_seed must be
      // provided iff |peer_identity| is not.
      base::Optional<base::span<const uint8_t, kQRSeedSize>> identity_seed);

  ~HandshakeInitiator();

  // BuildInitialMessage returns the handshake message to send to the peer to
  // start a handshake.
  std::vector<uint8_t> BuildInitialMessage();

  // ProcessResponse processes the handshake response from the peer. If
  // successful it returns a |Crypter| for protecting future messages on the
  // connection and a handshake transcript for signing over if needed.
  HandshakeResult ProcessResponse(base::span<const uint8_t> response);

 private:
  Noise noise_;
  std::array<uint8_t, 32> psk_;

  base::Optional<std::array<uint8_t, kP256X962Length>> peer_identity_;
  bssl::UniquePtr<EC_KEY> local_identity_;
  bssl::UniquePtr<EC_KEY> ephemeral_key_;
};

// RespondToHandshake responds to a caBLE v2 handshake started by a peer. Since
// the desktop speaks first in caBLE, this is called by the phone.
COMPONENT_EXPORT(DEVICE_FIDO)
HandshakeResult RespondToHandshake(
    // psk is derived from the connection nonce and either QR-code secrets or
    // pairing secrets.
    base::span<const uint8_t, 32> psk,
    // identity, if not nullptr, specifies that this is a paired handshake and
    // contains the phone's private key.
    bssl::UniquePtr<EC_KEY> identity,
    // peer_identity, which must be non-nullopt iff |identity| is nullptr,
    // contains the peer's public key as taken from the QR code.
    base::Optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
    // in contains the initial handshake message from the peer.
    base::span<const uint8_t> in,
    // out_response is set to the response handshake message, if successful.
    std::vector<uint8_t>* out_response);

// VerifyPairingSignature checks that |signature| is a valid signature of
// |handshake_hash| by |peer_public_key_x962|. This is used by a phone to prove
// possession of |peer_public_key_x962| since the |handshake_hash| encloses
// random values generated by the desktop and thus is a fresh value.
COMPONENT_EXPORT(DEVICE_FIDO)
bool VerifyPairingSignature(
    base::span<const uint8_t, kQRSeedSize> identity_seed,
    base::span<const uint8_t, kP256X962Length> peer_public_key_x962,
    base::span<const uint8_t, std::tuple_size<HandshakeHash>::value>
        handshake_hash,
    base::span<const uint8_t> signature);

// CalculatePairingSignature generates a value that will satisfy
// |VerifyPairingSignature|.
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> CalculatePairingSignature(
    const EC_KEY* identity_key,
    base::span<const uint8_t, kP256X962Length> peer_public_key_x962,
    base::span<const uint8_t, std::tuple_size<HandshakeHash>::value>
        handshake_hash);

}  // namespace cablev2
}  // namespace device

#endif  // DEVICE_FIDO_CABLE_V2_HANDSHAKE_H_
