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

// Base32Ord converts |c| into its base32 value, as defined in
// https://tools.ietf.org/html/rfc4648#section-6.
constexpr uint32_t Base32Ord(char c) {
  if (c >= 'a' && c <= 'z') {
    return c - 'a';
  } else if (c >= '2' && c <= '7') {
    return 26 + c - '2';
  } else {
    __builtin_unreachable();
  }
}

// TLD enumerates the set of possible top-level domains that a tunnel server can
// use.
enum class TLD {
  COM = 0,
  ORG = 1,
  NET = 2,
  INFO = 3,
};

// EncodeDomain converts a domain name, in the form of a four-letter, base32
// domain plus a TLD, into a 22-bit value.
constexpr uint32_t EncodeDomain(const char label[5], TLD tld) {
  const uint32_t tld_value = static_cast<uint32_t>(tld);
  if (tld_value > 3 || label[4] != 0) {
    __builtin_unreachable();
  }
  return ((Base32Ord(label[0]) << 15 | Base32Ord(label[1]) << 10 |
           Base32Ord(label[2]) << 5 | Base32Ord(label[3]))
          << 2) |
         tld_value;
}

// DecodeDomain converts a 22-bit tunnel server domain (as encoded by
// |EncodeDomain|) into a string in dotted form.
COMPONENT_EXPORT(DEVICE_FIDO) std::string DecodeDomain(uint32_t domain);

// GetNewTunnelURL converts a 22-bit tunnel server domain (as encoded by
// |EncodeDomain|), and a tunnel ID, into a WebSockets-based URL for creating a
// new tunnel.
COMPONENT_EXPORT(DEVICE_FIDO)
GURL GetNewTunnelURL(uint32_t domain, base::span<const uint8_t, 16> id);

// GetConnectURL converts a 22-bit tunnel server domain (as encoded by
// |EncodeDomain|), a routing-ID, and a tunnel ID, into a WebSockets-based URL
// for connecting to an existing tunnel.
COMPONENT_EXPORT(DEVICE_FIDO)
GURL GetConnectURL(uint32_t domain,
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

// TODO(agl): this could probably be a class.

// Components contains the parts of a decrypted EID.
struct Components {
  uint32_t tunnel_server_domain;
  std::array<uint8_t, kRoutingIdSize> routing_id;
  std::array<uint8_t, kNonceSize> nonce;
};

// FromComponents constructs a valid EID from the given components. |IsValid|
// will be true of the result.
COMPONENT_EXPORT(DEVICE_FIDO)
CableEidArray FromComponents(const Components& components);

// IsValid returns true if |eid| could have been produced by |FromComponents|.
COMPONENT_EXPORT(DEVICE_FIDO)
bool IsValid(const CableEidArray& eid);

// ToComponents explodes a decrypted EID into its components. It's the
// inverse of |ComponentsToEID|. |IsValid| must be true for the given EID before
// calling this function.
COMPONENT_EXPORT(DEVICE_FIDO)
Components ToComponents(const CableEidArray& eid);

}  // namespace eid

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

// HandshakeInitiator starts a caBLE v2 handshake and processes the single
// response message from the other party. The handshake is always initiated from
// the phone.
class COMPONENT_EXPORT(DEVICE_FIDO) HandshakeInitiator {
 public:
  HandshakeInitiator(
      // psk is derived from the connection nonce and either QR-code secrets
      // pairing secrets.
      base::span<const uint8_t, 32> psk,
      // peer_identity, if not nullopt, specifies that this is a QR handshake
      // and then contains a P-256 public key for the peer. Otherwise this is a
      // paired handshake.
      base::Optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
      // local_identity must be provided iff |peer_identity| is not. It contains
      // the local identity key.
      bssl::UniquePtr<EC_KEY> local_identity);

  ~HandshakeInitiator();

  // BuildInitialMessage returns the handshake message to send to the peer to
  // start a handshake.
  std::vector<uint8_t> BuildInitialMessage(
      // eid is the EID that was advertised for this handshake. This is checked
      // as part of the handshake.
      base::span<const uint8_t, kCableEphemeralIdSize> eid,
      // getinfo contains the CBOR-serialised getInfo response for this
      // authenticator. This is assumed not to contain highly-sensitive
      // information and is included to avoid an extra round-trip. (It is
      // encrypted but an attacker who could eavesdrop on the tunnel connection
      // and observe the QR code could obtain it.)
      base::span<const uint8_t> get_info_bytes);

  // ProcessResponse processes the handshake response from the peer. If
  // successful it returns a |Crypter| for protecting future messages on the
  // connection and a handshake transcript for signing over if needed.
  base::Optional<std::pair<std::unique_ptr<Crypter>, HandshakeHash>>
  ProcessResponse(base::span<const uint8_t> response);

 private:
  Noise noise_;
  std::array<uint8_t, 32> psk_;

  base::Optional<std::array<uint8_t, kP256X962Length>> peer_identity_;
  bssl::UniquePtr<EC_KEY> local_identity_;
  bssl::UniquePtr<EC_KEY> ephemeral_key_;
};

// ResponderResult is the result of a successful handshake from the responder's
// side. It contains a Crypter for protecting future messages, the contents of
// the getInfo response given by the peer, and a hash of the handshake
// transcript.
struct COMPONENT_EXPORT(DEVICE_FIDO) ResponderResult {
  ResponderResult(std::unique_ptr<Crypter>,
                  std::vector<uint8_t> getinfo_bytes,
                  HandshakeHash);
  ~ResponderResult();
  ResponderResult(const ResponderResult&) = delete;
  ResponderResult(ResponderResult&&);
  ResponderResult& operator=(const ResponderResult&) = delete;

  std::unique_ptr<Crypter> crypter;
  std::vector<uint8_t> getinfo_bytes;
  const HandshakeHash handshake_hash;
};

// RespondToHandshake responds to a caBLE v2 handshake started by a peer.
COMPONENT_EXPORT(DEVICE_FIDO)
base::Optional<ResponderResult> RespondToHandshake(
    // psk is derived from the connection nonce and either QR-code secrets or
    // pairing secrets.
    base::span<const uint8_t, 32> psk,
    // eid is the EID that was advertised for this handshake. This is checked
    // as part of the handshake.
    base::span<const uint8_t, kCableEphemeralIdSize> eid,
    // identity_seed, if not nullopt, specifies that this is a QR handshake and
    // contains the seed for QR key for this client.
    base::Optional<base::span<const uint8_t, kQRSeedSize>> identity_seed,
    // peer_identity, which must be non-nullopt iff |identity| is nullopt,
    // contains the peer's public key as taken from the pairing data.
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
