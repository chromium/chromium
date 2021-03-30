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
#include "third_party/boringssl/src/include/openssl/aes.h"
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

// ReservedBitsAreZero returns true if the currently unused bits in |eid| are
// all set to zero.
bool ReservedBitsAreZero(const CableEidArray& eid) {
  return eid[0] == 0;
}

bssl::UniquePtr<EC_KEY> ECKeyFromSeed(
    base::span<const uint8_t, kQRSeedSize> seed) {
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  return bssl::UniquePtr<EC_KEY>(
      EC_KEY_derive_from_secret(p256.get(), seed.data(), seed.size()));
}

}  // namespace

namespace tunnelserver {

std::string DecodeDomain(uint16_t domain) {
  char templ[] = "caBLEv2 tunnel server domain\x00\x00";
  memcpy(&templ[sizeof(templ) - 2], &domain, sizeof(domain));
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const uint8_t*>(templ), sizeof(templ), digest);
  uint64_t result;
  static_assert(sizeof(result) <= sizeof(digest), "");
  memcpy(&result, digest, sizeof(result));
  // This value causes the range of this function to intersect at a single point
  // with the function previously used. This allows us not to change the initial
  // tunnel server domain name.
  result ^= 0x35286e67508f8e42;

  static const char kBase32Chars[33] = "abcdefghijklmnopqrstuvwxyz234567";
  const int tld_value = result & 3;
  result >>= 2;

  std::string ret = "cable.";
  while (result != 0) {
    ret.push_back(kBase32Chars[result & 31]);
    result >>= 5;
  }
  ret.push_back('.');

  static const char kTLDs[4][5] = {"com", "org", "net", "info"};
  ret += kTLDs[tld_value];

  return ret;
}

GURL GetNewTunnelURL(uint16_t domain, base::span<const uint8_t, 16> id) {
  std::string ret = "wss://" + DecodeDomain(domain) + "/cable/new/";

  ret += base::HexEncode(id);
  const GURL url(ret);
  DCHECK(url.is_valid());
  return url;
}

GURL GetConnectURL(uint16_t domain,
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

std::array<uint8_t, kAdvertSize> Encrypt(
    const CableEidArray& eid,
    base::span<const uint8_t, kEIDKeySize> key) {
  // |eid| is encrypted as an AES block and a 4-byte HMAC is appended. The |key|
  // is a pair of 256-bit keys, concatenated.
  DCHECK(ReservedBitsAreZero(eid));

  std::array<uint8_t, kAdvertSize> ret;
  static_assert(EXTENT(ret) == AES_BLOCK_SIZE + 4, "");

  AES_KEY aes_key;
  static_assert(EXTENT(key) == 32 + 32, "");
  CHECK(AES_set_encrypt_key(key.data(), /*bits=*/8 * 32, &aes_key) == 0);
  static_assert(EXTENT(eid) == AES_BLOCK_SIZE, "EIDs are not AES blocks");
  AES_encrypt(/*in=*/eid.data(), /*out=*/ret.data(), &aes_key);

  uint8_t hmac[SHA256_DIGEST_LENGTH];
  unsigned hmac_len;
  CHECK(HMAC(EVP_sha256(), key.data() + 32, 32, ret.data(), AES_BLOCK_SIZE,
             hmac, &hmac_len) != nullptr);
  CHECK_EQ(hmac_len, sizeof(hmac));

  static_assert(sizeof(hmac) >= 4, "");
  memcpy(ret.data() + AES_BLOCK_SIZE, hmac, 4);

  return ret;
}

base::Optional<CableEidArray> Decrypt(
    const std::array<uint8_t, kAdvertSize>& advert,
    base::span<const uint8_t, kEIDKeySize> key) {
  // See |Encrypt| about the format.
  static_assert(EXTENT(advert) == AES_BLOCK_SIZE + 4, "");
  static_assert(EXTENT(key) == 32 + 32, "");

  uint8_t calculated_hmac[SHA256_DIGEST_LENGTH];
  unsigned calculated_hmac_len;
  CHECK(HMAC(EVP_sha256(), key.data() + 32, 32, advert.data(), AES_BLOCK_SIZE,
             calculated_hmac, &calculated_hmac_len) != nullptr);
  CHECK_EQ(calculated_hmac_len, sizeof(calculated_hmac));

  if (CRYPTO_memcmp(calculated_hmac, advert.data() + AES_BLOCK_SIZE, 4) != 0) {
    return base::nullopt;
  }

  AES_KEY aes_key;
  CHECK(AES_set_decrypt_key(key.data(), /*bits=*/8 * 32, &aes_key) == 0);
  CableEidArray plaintext;
  static_assert(EXTENT(plaintext) == AES_BLOCK_SIZE, "EIDs are not AES blocks");
  AES_decrypt(/*in=*/advert.data(), /*out=*/plaintext.data(), &aes_key);

  // Ensure that reserved bits are zero. They might be used for new features in
  // the future but support for those features must be advertised in the QR
  // code, thus authenticators should not be unilaterally setting any of these
  // bits.
  if (!ReservedBitsAreZero(plaintext)) {
    return base::nullopt;
  }

  return plaintext;
}

CableEidArray FromComponents(const Components& components) {
  CableEidArray eid;
  static_assert(EXTENT(components.nonce) == kNonceSize, "");
  static_assert(EXTENT(eid) == 1 + kNonceSize + sizeof(components.routing_id) +
                                   sizeof(components.tunnel_server_domain),
                "");

  eid[0] = 0;
  memcpy(&eid[1], components.nonce.data(), kNonceSize);
  memcpy(&eid[1 + kNonceSize], components.routing_id.data(),
         sizeof(components.routing_id));
  memcpy(&eid[1 + kNonceSize + sizeof(components.routing_id)],
         &components.tunnel_server_domain,
         sizeof(components.tunnel_server_domain));

  return eid;
}

Components ToComponents(const CableEidArray& eid) {
  Components ret;
  static_assert(EXTENT(ret.nonce) == kNonceSize, "");
  static_assert(EXTENT(eid) == 1 + kNonceSize + sizeof(ret.routing_id) +
                                   sizeof(ret.tunnel_server_domain),
                "");

  memcpy(ret.nonce.data(), &eid[1], kNonceSize);
  memcpy(ret.routing_id.data(), &eid[1 + kNonceSize], sizeof(ret.routing_id));
  memcpy(&ret.tunnel_server_domain,
         &eid[1 + kNonceSize + sizeof(ret.routing_id)],
         sizeof(ret.tunnel_server_domain));

  return ret;
}

}  // namespace eid

namespace qr {

constexpr char kPrefix[] = "fido://";

// DecompressPublicKey converts a compressed public key (from a scanned QR
// code) into a standard, uncompressed one.
static base::Optional<std::array<uint8_t, device::kP256X962Length>>
DecompressPublicKey(base::span<const uint8_t> compressed_public_key) {
  if (compressed_public_key.size() !=
      device::cablev2::kCompressedPublicKeySize) {
    return base::nullopt;
  }

  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), compressed_public_key.data(),
                          compressed_public_key.size(), /*ctx=*/nullptr)) {
    return base::nullopt;
  }
  std::array<uint8_t, device::kP256X962Length> ret;
  CHECK_EQ(
      ret.size(),
      EC_POINT_point2oct(p256.get(), point.get(), POINT_CONVERSION_UNCOMPRESSED,
                         ret.data(), ret.size(), /*ctx=*/nullptr));
  return ret;
}

static std::array<uint8_t, device::cablev2::kCompressedPublicKeySize>
SeedToCompressedPublicKey(base::span<const uint8_t, 32> seed) {
  bssl::UniquePtr<EC_KEY> key = ECKeyFromSeed(seed);
  const EC_POINT* public_key = EC_KEY_get0_public_key(key.get());

  std::array<uint8_t, device::cablev2::kCompressedPublicKeySize> ret;
  CHECK_EQ(ret.size(),
           EC_POINT_point2oct(EC_KEY_get0_group(key.get()), public_key,
                              POINT_CONVERSION_COMPRESSED, ret.data(),
                              ret.size(), /*ctx=*/nullptr));
  return ret;
}

// static
base::Optional<Components> Parse(const std::string& qr_url) {
  if (qr_url.find(kPrefix) != 0) {
    return base::nullopt;
  }

  base::StringPiece qr_url_base64(qr_url);
  qr_url_base64 = qr_url_base64.substr(sizeof(kPrefix) - 1);
  std::string qr_data_str;
  if (!base::Base64UrlDecode(qr_url_base64,
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &qr_data_str)) {
    return base::nullopt;
  }

  base::Optional<cbor::Value> qr_contents =
      cbor::Reader::Read(base::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(qr_data_str.data()),
          qr_data_str.size()));
  if (!qr_contents || !qr_contents->is_map()) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& qr_contents_map(qr_contents->GetMap());

  base::span<const uint8_t> values[2];
  for (size_t i = 0; i < base::size(values); i++) {
    const cbor::Value::MapValue::const_iterator it =
        qr_contents_map.find(cbor::Value(static_cast<int>(i)));
    if (it == qr_contents_map.end() || !it->second.is_bytestring()) {
      return base::nullopt;
    }
    values[i] = it->second.GetBytestring();
  }

  base::span<const uint8_t> compressed_public_key = values[0];
  base::span<const uint8_t> qr_secret = values[1];

  Components ret;
  if (qr_secret.size() != ret.secret.size()) {
    return base::nullopt;
  }
  std::copy(qr_secret.begin(), qr_secret.end(), ret.secret.begin());

  base::Optional<std::array<uint8_t, device::kP256X962Length>> peer_identity =
      DecompressPublicKey(compressed_public_key);
  if (!peer_identity) {
    FIDO_LOG(ERROR) << "Invalid compressed public key in QR data";
    return base::nullopt;
  }

  ret.peer_identity = *peer_identity;
  return ret;
}

std::string Encode(base::span<const uint8_t, kQRKeySize> qr_key) {
  cbor::Value::MapValue qr_contents;
  qr_contents.emplace(
      0, SeedToCompressedPublicKey(
             base::span<const uint8_t, device::cablev2::kQRSeedSize>(
                 qr_key.data(), device::cablev2::kQRSeedSize)));

  qr_contents.emplace(1, qr_key.subspan(device::cablev2::kQRSeedSize));

  const base::Optional<std::vector<uint8_t>> qr_data =
      cbor::Writer::Write(cbor::Value(std::move(qr_contents)));

  std::string base64_qr_data;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(qr_data->data()),
                        qr_data->size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &base64_qr_data);

  return std::string(kPrefix) + base64_qr_data;
}

}  // namespace qr

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
    base::Optional<base::span<const uint8_t, kQRSeedSize>> identity_seed)
    : psk_(fido_parsing_utils::Materialize(psk)),
      local_identity_(identity_seed ? ECKeyFromSeed(*identity_seed) : nullptr) {
  DCHECK(peer_identity.has_value() ^ static_cast<bool>(local_identity_));
  if (peer_identity) {
    peer_identity_ =
        fido_parsing_utils::Materialize<kP256X962Length>(*peer_identity);
  }
}

HandshakeInitiator::~HandshakeInitiator() = default;

std::vector<uint8_t> HandshakeInitiator::BuildInitialMessage() {
  uint8_t prologue[1];

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

  std::vector<uint8_t> ciphertext =
      noise_.EncryptAndHash(base::span<const uint8_t>());

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

HandshakeResult HandshakeInitiator::ProcessResponse(
    base::span<const uint8_t> response) {
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

HandshakeResult RespondToHandshake(
    base::span<const uint8_t, 32> psk,
    bssl::UniquePtr<EC_KEY> identity,
    base::Optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
    base::span<const uint8_t> in,
    std::vector<uint8_t>* out_response) {
  DCHECK(peer_identity.has_value() ^ static_cast<bool>(identity));

  if (in.size() < kP256X962Length) {
    FIDO_LOG(DEBUG) << "Handshake truncated (" << in.size() << " bytes)";
    return base::nullopt;
  }
  auto peer_point_bytes = in.subspan(0, kP256X962Length);
  auto ciphertext = in.subspan(kP256X962Length);

  Noise noise;
  uint8_t prologue[1];
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
  if (!plaintext || !plaintext->empty()) {
    FIDO_LOG(DEBUG) << "Failed to decrypt handshake ciphertext.";
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
  return std::make_pair(std::make_unique<cablev2::Crypter>(read_key, write_key),
                        noise.handshake_hash());
}

bool VerifyPairingSignature(
    base::span<const uint8_t, kQRSeedSize> identity_seed,
    base::span<const uint8_t, kP256X962Length> peer_public_key_x962,
    base::span<const uint8_t, std::tuple_size<HandshakeHash>::value>
        handshake_hash,
    base::span<const uint8_t> signature) {
  bssl::UniquePtr<EC_KEY> identity_key = ECKeyFromSeed(identity_seed);
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
