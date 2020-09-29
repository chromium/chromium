// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_handshake.h"

#include <array>
#include <type_traits>

#include "base/base64url.h"
#include "base/bits.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#include "url/gurl.h"

namespace device {
namespace cablev2 {

namespace {

// Maximum value of a sequence number. Exceeding this causes all operations to
// return an error. This is assumed to be vastly larger than any caBLE exchange
// will ever reach.
constexpr uint32_t kMaxSequence = (1 << 24) - 1;

bool ConstructNonce(uint32_t counter, base::span<uint8_t, 12> out_nonce) {
  if (counter > kMaxSequence) {
    return false;
  }

  // Nonce is just a little-endian counter.
  std::array<uint8_t, sizeof(counter)> counter_bytes;
  memcpy(counter_bytes.data(), &counter, sizeof(counter));
  std::copy(counter_bytes.begin(), counter_bytes.end(), out_nonce.begin());
  std::fill(out_nonce.begin() + counter_bytes.size(), out_nonce.end(), 0);
  return true;
}

std::array<uint8_t, 32> PairingSignature(
    const EC_KEY* identity_key,
    base::span<const uint8_t, kP256X962Length> peer_public_key_x962,
    base::span<const uint8_t, std::tuple_size<HandshakeHash>::value>
        handshake_hash) {
  const EC_GROUP* const p256 = EC_KEY_get0_group(identity_key);
  bssl::UniquePtr<EC_POINT> peer_public_key(EC_POINT_new(p256));
  CHECK(EC_POINT_oct2point(p256, peer_public_key.get(),
                           peer_public_key_x962.data(),
                           peer_public_key_x962.size(),
                           /*ctx=*/nullptr));
  uint8_t shared_secret[32];
  CHECK(ECDH_compute_key(shared_secret, sizeof(shared_secret),
                         peer_public_key.get(), identity_key,
                         /*kdf=*/nullptr) == sizeof(shared_secret));

  std::array<uint8_t, SHA256_DIGEST_LENGTH> expected_signature;
  unsigned expected_signature_len = 0;
  CHECK(HMAC(EVP_sha256(), /*key=*/shared_secret, sizeof(shared_secret),
             handshake_hash.data(), handshake_hash.size(),
             expected_signature.data(), &expected_signature_len) != nullptr);
  CHECK_EQ(expected_signature_len, EXTENT(expected_signature));
  return expected_signature;
}

}  // namespace

namespace tunnelserver {

std::string DecodeDomain(uint32_t domain) {
  static const char kBase32Chars[33] = "abcdefghijklmnopqrstuvwxyz234567";

  std::string ret;
  ret.push_back(kBase32Chars[(domain >> 17) & 0x1f]);
  ret.push_back(kBase32Chars[(domain >> 12) & 0x1f]);
  ret.push_back(kBase32Chars[(domain >> 7) & 0x1f]);
  ret.push_back(kBase32Chars[(domain >> 2) & 0x1f]);
  ret.push_back('.');

  static const char kTLDs[4][5] = {"com", "org", "net", "info"};
  ret += kTLDs[domain & 3];

  return ret;
}

GURL GetNewTunnelURL(uint32_t domain, base::span<const uint8_t, 16> id) {
  std::string ret = "wss://" + DecodeDomain(domain) + "/cable/new/";

  ret += base::HexEncode(id);
  const GURL url(ret);
  DCHECK(url.is_valid());
  return url;
}

GURL GetConnectURL(uint32_t domain,
                   std::array<uint8_t, kRoutingIdSize> routing_id,
                   base::span<const uint8_t, 16> id) {
  std::string ret = "wss://" + DecodeDomain(domain) + "/cable/connect/";

  ret += base::HexEncode(routing_id);
  ret += "/";
  ret += base::HexEncode(id);

  const GURL url(ret);
  DCHECK(url.is_valid());
  return url;
}

GURL GetContactURL(const std::string& tunnel_server,
                   base::span<const uint8_t> contact_id) {
  std::string contact_id_base64;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(contact_id.data()),
                        contact_id.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &contact_id_base64);
  GURL ret(std::string("wss://") + tunnel_server + "/cable/contact/" +
           contact_id_base64);
  DCHECK(ret.is_valid());
  return ret;
}

}  // namespace tunnelserver

namespace eid {

CableEidArray FromComponents(const Components& components) {
  DCHECK_EQ(components.tunnel_server_domain >> 22, 0u);

  CableEidArray eid;
  static_assert(EXTENT(eid) == 16, "");
  eid[0] = components.tunnel_server_domain;
  eid[1] = components.tunnel_server_domain >> 8;
  eid[2] = components.tunnel_server_domain >> 16;
  static_assert(EXTENT(components.routing_id) == 3, "");
  eid[3] = components.routing_id[0];
  eid[4] = components.routing_id[1];
  eid[5] = components.routing_id[2];
  eid[6] = 0;
  eid[7] = 0;
  static_assert(EXTENT(eid) == 8 + kNonceSize, "");
  memcpy(&eid[8], components.nonce.data(), kNonceSize);

  return eid;
}

bool IsValid(const CableEidArray& eid) {
  static_assert(EXTENT(eid) >= 8, "");
  return eid[6] == 0 && eid[7] == 0 && (eid[2] >> 6) == 0;
}

Components ToComponents(const CableEidArray& eid) {
  DCHECK(IsValid(eid));

  Components ret;
  static_assert(EXTENT(eid) == 16, "");
  ret.tunnel_server_domain = static_cast<uint32_t>(eid[0]) |
                             (static_cast<uint32_t>(eid[1]) << 8) |
                             ((static_cast<uint32_t>(eid[2]) & 0x3f) << 16);
  ret.routing_id[0] = eid[3];
  ret.routing_id[1] = eid[4];
  ret.routing_id[2] = eid[5];

  static_assert(EXTENT(eid) == 8 + kNonceSize, "");
  memcpy(ret.nonce.data(), &eid[8], kNonceSize);
  return ret;
}

}  // namespace eid

namespace internal {

void Derive(uint8_t* out,
            size_t out_len,
            base::span<const uint8_t> secret,
            base::span<const uint8_t> nonce,
            DerivedValueType type) {
  static_assert(sizeof(DerivedValueType) <= sizeof(uint32_t), "");
  const uint32_t type32 = static_cast<uint32_t>(type);

  HKDF(out, out_len, EVP_sha256(), secret.data(), secret.size(),
       /*salt=*/nonce.data(), nonce.size(),
       /*info=*/reinterpret_cast<const uint8_t*>(&type32), sizeof(type32));
}

}  // namespace internal

base::Optional<std::vector<uint8_t>> EncodePaddedCBORMap(
    cbor::Value::MapValue map) {
  // TODO: this should pad to 1K, not 256 bytes.
  base::Optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  if (!cbor_bytes) {
    return base::nullopt;
  }

  base::CheckedNumeric<size_t> padded_size_checked = cbor_bytes->size();
  padded_size_checked += 1;  // padding-length byte
  padded_size_checked = (padded_size_checked + 255) & ~255;
  if (!padded_size_checked.IsValid()) {
    return base::nullopt;
  }

  const size_t padded_size = padded_size_checked.ValueOrDie();
  DCHECK_GT(padded_size, cbor_bytes->size());
  const size_t extra_padding = padded_size - cbor_bytes->size();

  cbor_bytes->resize(padded_size);
  DCHECK_LE(extra_padding, 256u);
  cbor_bytes->at(padded_size - 1) = static_cast<uint8_t>(extra_padding - 1);

  return *cbor_bytes;
}

base::Optional<cbor::Value> DecodePaddedCBORMap(
    base::span<const uint8_t> input) {
  if (input.empty()) {
    return base::nullopt;
  }

  const size_t padding_length = input.back();
  if (padding_length + 1 > input.size()) {
    FIDO_LOG(DEBUG) << "Invalid padding in caBLE handshake message";
    return base::nullopt;
  }
  input = input.subspan(0, input.size() - padding_length - 1);

  base::Optional<cbor::Value> payload = cbor::Reader::Read(input);
  if (!payload || !payload->is_map()) {
    FIDO_LOG(DEBUG) << "CBOR parse failure in caBLE handshake message";
    return base::nullopt;
  }

  return payload;
}

Crypter::Crypter(base::span<const uint8_t, 32> read_key,
                 base::span<const uint8_t, 32> write_key)
    : read_key_(fido_parsing_utils::Materialize(read_key)),
      write_key_(fido_parsing_utils::Materialize(write_key)) {}

Crypter::~Crypter() = default;

bool Crypter::Encrypt(std::vector<uint8_t>* message_to_encrypt) {
  // Messages will be padded in order to round their length up to a multiple
  // of kPaddingGranularity.
  constexpr size_t kPaddingGranularity = 32;
  static_assert(kPaddingGranularity < 256, "padding too large");
  static_assert(base::bits::IsPowerOfTwo(kPaddingGranularity),
                "padding must be a power of two");

  // Padding consists of a some number of zero bytes appended to the message
  // and the final byte in the message is the number of zeros.
  base::CheckedNumeric<size_t> padded_size_checked = message_to_encrypt->size();
  padded_size_checked += 1;  // padding-length byte.
  padded_size_checked = (padded_size_checked + kPaddingGranularity - 1) &
                        ~(kPaddingGranularity - 1);
  if (!padded_size_checked.IsValid()) {
    NOTREACHED();
    return false;
  }

  const size_t padded_size = padded_size_checked.ValueOrDie();
  CHECK_GT(padded_size, message_to_encrypt->size());
  const size_t num_zeros = padded_size - message_to_encrypt->size() - 1;

  std::vector<uint8_t> padded_message(padded_size, 0);
  memcpy(padded_message.data(), message_to_encrypt->data(),
         message_to_encrypt->size());
  // The number of added zeros has to fit in a single byte so it has to be
  // less than 256.
  DCHECK_LT(num_zeros, 256u);
  padded_message[padded_message.size() - 1] = static_cast<uint8_t>(num_zeros);

  std::array<uint8_t, 12> nonce;
  if (!ConstructNonce(write_sequence_num_++, nonce)) {
    return false;
  }

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(write_key_);
  DCHECK_EQ(nonce.size(), aes_key.NonceLength());

  const uint8_t additional_data[1] = {/*version=*/2};
  std::vector<uint8_t> ciphertext =
      aes_key.Seal(padded_message, nonce, additional_data);
  message_to_encrypt->swap(ciphertext);
  return true;
}

bool Crypter::Decrypt(base::span<const uint8_t> ciphertext,
                      std::vector<uint8_t>* out_plaintext) {
  std::array<uint8_t, 12> nonce;
  if (!ConstructNonce(read_sequence_num_, nonce)) {
    return false;
  }

  crypto::Aead aes_key(crypto::Aead::AES_256_GCM);
  aes_key.Init(read_key_);
  DCHECK_EQ(nonce.size(), aes_key.NonceLength());

  const uint8_t additional_data[1] = {/*version=*/2};
  base::Optional<std::vector<uint8_t>> plaintext =
      aes_key.Open(ciphertext, nonce, additional_data);

  if (!plaintext) {
    return false;
  }
  read_sequence_num_++;

  if (plaintext->empty()) {
    FIDO_LOG(ERROR) << "Invalid caBLE message.";
    return false;
  }

  const size_t padding_length = (*plaintext)[plaintext->size() - 1];
  if (padding_length + 1 > plaintext->size()) {
    FIDO_LOG(ERROR) << "Invalid caBLE message.";
    return false;
  }
  plaintext->resize(plaintext->size() - padding_length - 1);

  out_plaintext->swap(*plaintext);
  return true;
}

bool Crypter::IsCounterpartyOfForTesting(const Crypter& other) const {
  return read_key_ == other.write_key_ && write_key_ == other.read_key_;
}

HandshakeInitiator::HandshakeInitiator(
    base::span<const uint8_t, 32> psk,
    base::Optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
    bssl::UniquePtr<EC_KEY> local_identity)
    : psk_(fido_parsing_utils::Materialize(psk)),
      local_identity_(std::move(local_identity)) {
  DCHECK(peer_identity.has_value() ^ static_cast<bool>(local_identity_));
  if (peer_identity) {
    peer_identity_ =
        fido_parsing_utils::Materialize<kP256X962Length>(*peer_identity);
  }
}

HandshakeInitiator::~HandshakeInitiator() = default;

std::vector<uint8_t> HandshakeInitiator::BuildInitialMessage(
    base::span<const uint8_t, kCableEphemeralIdSize> eid,
    base::span<const uint8_t> get_info_bytes) {
  uint8_t prologue[1 + kCableEphemeralIdSize];
  DCHECK_EQ(kCableEphemeralIdSize, eid.size());
  memcpy(&prologue[1], eid.data(), kCableEphemeralIdSize);

  if (peer_identity_) {
    noise_.Init(Noise::HandshakeType::kNKpsk0);
    prologue[0] = 0;
    noise_.MixHash(prologue);
    noise_.MixHash(*peer_identity_);
  } else {
    noise_.Init(Noise::HandshakeType::kKNpsk0);
    prologue[0] = 1;
    noise_.MixHash(prologue);
    noise_.MixHashPoint(EC_KEY_get0_public_key(local_identity_.get()));
  }

  noise_.MixKeyAndHash(psk_);
  ephemeral_key_.reset(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key_.get());
  CHECK(EC_KEY_generate_key(ephemeral_key_.get()));
  uint8_t ephemeral_key_public_bytes[kP256X962Length];
  CHECK_EQ(sizeof(ephemeral_key_public_bytes),
           EC_POINT_point2oct(
               group, EC_KEY_get0_public_key(ephemeral_key_.get()),
               POINT_CONVERSION_UNCOMPRESSED, ephemeral_key_public_bytes,
               sizeof(ephemeral_key_public_bytes), /*ctx=*/nullptr));
  noise_.MixHash(ephemeral_key_public_bytes);
  noise_.MixKey(ephemeral_key_public_bytes);

  if (peer_identity_) {
    // If we know the identity of the peer from a previous interaction, NKpsk0
    // is performed to ensure that other browsers, which may also know the PSK,
    // cannot impersonate the authenticator.
    bssl::UniquePtr<EC_POINT> peer_identity_point(EC_POINT_new(group));
    uint8_t es_key[32];
    CHECK(EC_POINT_oct2point(group, peer_identity_point.get(),
                             peer_identity_->data(), peer_identity_->size(),
                             /*ctx=*/nullptr));
    CHECK(ECDH_compute_key(es_key, sizeof(es_key), peer_identity_point.get(),
                           ephemeral_key_.get(),
                           /*kdf=*/nullptr) == sizeof(es_key));
    noise_.MixKey(es_key);
  }

  cbor::Value::MapValue payload;
  payload.emplace(0, get_info_bytes);
  base::Optional<std::vector<uint8_t>> plaintext =
      EncodePaddedCBORMap(std::move(payload));
  if (!plaintext) {
    FIDO_LOG(ERROR) << "Failed to pad getInfo response";
    return {};
  }
  std::vector<uint8_t> ciphertext = noise_.EncryptAndHash(*plaintext);

  std::vector<uint8_t> handshake_message;
  handshake_message.reserve(sizeof(ephemeral_key_public_bytes) +
                            ciphertext.size());
  handshake_message.insert(
      handshake_message.end(), ephemeral_key_public_bytes,
      ephemeral_key_public_bytes + sizeof(ephemeral_key_public_bytes));
  handshake_message.insert(handshake_message.end(), ciphertext.begin(),
                           ciphertext.end());

  return handshake_message;
}

base::Optional<std::pair<std::unique_ptr<Crypter>, HandshakeHash>>
HandshakeInitiator::ProcessResponse(base::span<const uint8_t> response) {
  if (response.size() < kP256X962Length) {
    FIDO_LOG(DEBUG) << "Handshake response truncated (" << response.size()
                    << " bytes)";
    return base::nullopt;
  }
  auto peer_point_bytes = response.subspan(0, kP256X962Length);
  auto ciphertext = response.subspan(kP256X962Length);

  bssl::UniquePtr<EC_POINT> peer_point(
      EC_POINT_new(EC_KEY_get0_group(ephemeral_key_.get())));
  uint8_t shared_key_ee[32];
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key_.get());
  if (!EC_POINT_oct2point(group, peer_point.get(), peer_point_bytes.data(),
                          peer_point_bytes.size(), /*ctx=*/nullptr) ||
      ECDH_compute_key(shared_key_ee, sizeof(shared_key_ee), peer_point.get(),
                       ephemeral_key_.get(),
                       /*kdf=*/nullptr) != sizeof(shared_key_ee)) {
    FIDO_LOG(DEBUG) << "Peer's P-256 point not on curve.";
    return base::nullopt;
  }

  noise_.MixHash(peer_point_bytes);
  noise_.MixKey(peer_point_bytes);
  noise_.MixKey(shared_key_ee);

  if (local_identity_) {
    uint8_t shared_key_se[32];
    if (ECDH_compute_key(shared_key_se, sizeof(shared_key_se), peer_point.get(),
                         local_identity_.get(),
                         /*kdf=*/nullptr) != sizeof(shared_key_se)) {
      FIDO_LOG(DEBUG) << "ECDH_compute_key failed";
      return base::nullopt;
    }
    noise_.MixKey(shared_key_se);
  }

  auto plaintext = noise_.DecryptAndHash(ciphertext);
  if (!plaintext || !plaintext->empty()) {
    FIDO_LOG(DEBUG) << "Invalid caBLE handshake message";
    return base::nullopt;
  }

  std::array<uint8_t, 32> read_key, write_key;
  std::tie(write_key, read_key) = noise_.traffic_keys();
  return std::make_pair(std::make_unique<cablev2::Crypter>(read_key, write_key),
                        noise_.handshake_hash());
}

ResponderResult::ResponderResult(std::unique_ptr<Crypter> in_crypter,
                                 std::vector<uint8_t> in_getinfo_bytes,
                                 HandshakeHash in_handshake_hash)
    : crypter(std::move(in_crypter)),
      getinfo_bytes(std::move(in_getinfo_bytes)),
      handshake_hash(in_handshake_hash) {}
ResponderResult::~ResponderResult() = default;
ResponderResult::ResponderResult(ResponderResult&&) = default;

base::Optional<ResponderResult> RespondToHandshake(
    base::span<const uint8_t, 32> psk,
    base::span<const uint8_t, kCableEphemeralIdSize> eid,
    base::Optional<base::span<const uint8_t, kQRSeedSize>> identity_seed,
    base::Optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
    base::span<const uint8_t> in,
    std::vector<uint8_t>* out_response) {
  DCHECK(peer_identity.has_value() ^ identity_seed.has_value());

  if (in.size() < kP256X962Length) {
    FIDO_LOG(DEBUG) << "Handshake truncated (" << in.size() << " bytes)";
    return base::nullopt;
  }
  auto peer_point_bytes = in.subspan(0, kP256X962Length);
  auto ciphertext = in.subspan(kP256X962Length);

  bssl::UniquePtr<EC_KEY> identity;
  if (identity_seed) {
    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    identity.reset(EC_KEY_derive_from_secret(p256.get(), identity_seed->data(),
                                             identity_seed->size()));
  }

  Noise noise;
  uint8_t prologue[1 + EXTENT(eid)];
  memcpy(&prologue[1], eid.data(), eid.size());
  if (identity) {
    noise.Init(device::Noise::HandshakeType::kNKpsk0);
    prologue[0] = 0;
    noise.MixHash(prologue);
    noise.MixHashPoint(EC_KEY_get0_public_key(identity.get()));
  } else {
    noise.Init(device::Noise::HandshakeType::kKNpsk0);
    prologue[0] = 1;
    noise.MixHash(prologue);
    noise.MixHash(*peer_identity);
  }

  noise.MixKeyAndHash(psk);
  noise.MixHash(peer_point_bytes);
  noise.MixKey(peer_point_bytes);

  bssl::UniquePtr<EC_KEY> ephemeral_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key.get());
  CHECK(EC_KEY_generate_key(ephemeral_key.get()));
  bssl::UniquePtr<EC_POINT> peer_point(EC_POINT_new(group));
  if (!EC_POINT_oct2point(group, peer_point.get(), peer_point_bytes.data(),
                          peer_point_bytes.size(),
                          /*ctx=*/nullptr)) {
    FIDO_LOG(DEBUG) << "Peer's P-256 point not on curve.";
    return base::nullopt;
  }

  if (identity) {
    uint8_t es_key[32];
    if (ECDH_compute_key(es_key, sizeof(es_key), peer_point.get(),
                         identity.get(),
                         /*kdf=*/nullptr) != sizeof(es_key)) {
      return base::nullopt;
    }
    noise.MixKey(es_key);
  }

  auto plaintext = noise.DecryptAndHash(ciphertext);
  if (!plaintext) {
    FIDO_LOG(DEBUG) << "Failed to decrypt handshake ciphertext.";
    return base::nullopt;
  }

  base::Optional<cbor::Value> payload(DecodePaddedCBORMap(*plaintext));
  if (!payload) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& payload_map(payload->GetMap());
  const auto getinfo_it = payload_map.find(cbor::Value(0));
  if (getinfo_it == payload_map.end() || !getinfo_it->second.is_bytestring()) {
    FIDO_LOG(DEBUG) << "CBOR structure error in caBLE handshake message";
    return base::nullopt;
  }

  uint8_t ephemeral_key_public_bytes[kP256X962Length];
  CHECK_EQ(sizeof(ephemeral_key_public_bytes),
           EC_POINT_point2oct(
               group, EC_KEY_get0_public_key(ephemeral_key.get()),
               POINT_CONVERSION_UNCOMPRESSED, ephemeral_key_public_bytes,
               sizeof(ephemeral_key_public_bytes),
               /*ctx=*/nullptr));
  noise.MixHash(ephemeral_key_public_bytes);
  noise.MixKey(ephemeral_key_public_bytes);

  uint8_t shared_key_ee[32];
  if (ECDH_compute_key(shared_key_ee, sizeof(shared_key_ee), peer_point.get(),
                       ephemeral_key.get(),
                       /*kdf=*/nullptr) != sizeof(shared_key_ee)) {
    return base::nullopt;
  }
  noise.MixKey(shared_key_ee);

  if (peer_identity) {
    bssl::UniquePtr<EC_POINT> peer_identity_point(EC_POINT_new(group));
    CHECK(EC_POINT_oct2point(group, peer_identity_point.get(),
                             peer_identity->data(), peer_identity->size(),
                             /*ctx=*/nullptr));
    uint8_t shared_key_se[32];
    if (ECDH_compute_key(shared_key_se, sizeof(shared_key_se),
                         peer_identity_point.get(), ephemeral_key.get(),
                         /*kdf=*/nullptr) != sizeof(shared_key_se)) {
      return base::nullopt;
    }
    noise.MixKey(shared_key_se);
  }

  const std::vector<uint8_t> my_ciphertext =
      noise.EncryptAndHash(base::span<const uint8_t>());
  out_response->insert(
      out_response->end(), ephemeral_key_public_bytes,
      ephemeral_key_public_bytes + sizeof(ephemeral_key_public_bytes));
  out_response->insert(out_response->end(), my_ciphertext.begin(),
                       my_ciphertext.end());

  std::array<uint8_t, 32> read_key, write_key;
  std::tie(read_key, write_key) = noise.traffic_keys();
  return base::make_optional<ResponderResult>(
      std::make_unique<Crypter>(read_key, write_key),
      getinfo_it->second.GetBytestring(), noise.handshake_hash());
}

bool VerifyPairingSignature(
    base::span<const uint8_t, kQRSeedSize> identity_seed,
    base::span<const uint8_t, kP256X962Length> peer_public_key_x962,
    base::span<const uint8_t, std::tuple_size<HandshakeHash>::value>
        handshake_hash,
    base::span<const uint8_t> signature) {
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> identity_key(EC_KEY_derive_from_secret(
      p256.get(), identity_seed.data(), identity_seed.size()));

  std::array<uint8_t, SHA256_DIGEST_LENGTH> expected_signature =
      PairingSignature(identity_key.get(), peer_public_key_x962,
                       handshake_hash);
  return signature.size() == EXTENT(expected_signature) &&
         CRYPTO_memcmp(expected_signature.data(), signature.data(),
                       EXTENT(expected_signature)) == 0;
}

std::vector<uint8_t> CalculatePairingSignature(
    const EC_KEY* identity_key,
    base::span<const uint8_t, kP256X962Length> peer_public_key_x962,
    base::span<const uint8_t, std::tuple_size<HandshakeHash>::value>
        handshake_hash) {
  std::array<uint8_t, SHA256_DIGEST_LENGTH> expected_signature =
      PairingSignature(identity_key, peer_public_key_x962, handshake_hash);
  return std::vector<uint8_t>(expected_signature.begin(),
                              expected_signature.end());
}

}  // namespace cablev2
}  // namespace device
