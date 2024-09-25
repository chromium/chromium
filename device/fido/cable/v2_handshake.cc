// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/cable/v2_handshake.h"

#include <inttypes.h>

#include <algorithm>
#include <array>
#include <bit>
#include <type_traits>

#include "base/base64url.h"
#include "base/functional/overloaded.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "device/fido/cable/v2_constants.h"
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

bool ConstructNonce(uint32_t counter, base::span<uint8_t, 12u> out_nonce) {
  if (counter > kMaxSequence) {
    return false;
  }

  auto [zeros, counter_span] = out_nonce.split_at<12u - 4u>();
  std::ranges::fill(zeros, uint8_t{0});
  counter_span.copy_from(base::numerics::U32ToBigEndian(counter));
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

}  // namespace

namespace tunnelserver {

// kAssignedDomains is the list of defined tunnel server domains. These map
// to values 0..256.
static const char* kAssignedDomains[] = {"cable.ua5v.com", "cable.auth.com"};

std::optional<KnownDomainID> ToKnownDomainID(uint16_t domain) {
  if (domain >= 256 || domain < std::size(kAssignedDomains)) {
    return KnownDomainID(domain);
  }
  return std::nullopt;
}

std::string DecodeDomain(KnownDomainID domain_id) {
  const uint16_t domain = domain_id.value();
  if (domain < 256) {
    // The |KnownDomainID| type should only contain valid values for this but,
    // just in case, CHECK it too.
    CHECK_LT(domain, std::size(kAssignedDomains));
    return kAssignedDomains[domain];
  }

  char templ[] = "caBLEv2 tunnel server domain\x00\x00";
  memcpy(&templ[sizeof(templ) - 1 - sizeof(domain)], &domain, sizeof(domain));
  uint8_t digest[SHA256_DIGEST_LENGTH];
  // The input should be NUL-terminated, thus the trailing NUL in |templ| is
  // included here.
  SHA256(reinterpret_cast<const uint8_t*>(templ), sizeof(templ), digest);
  uint64_t result;
  static_assert(sizeof(result) <= sizeof(digest), "");
  memcpy(&result, digest, sizeof(result));

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

GURL GetNewTunnelURL(KnownDomainID domain, base::span<const uint8_t, 16> id) {
  std::string ret = "wss://" + DecodeDomain(domain) + "/cable/new/";

  ret += base::HexEncode(id);
  const GURL url(ret);
  DCHECK(url.is_valid());
  return url;
}

GURL GetConnectURL(KnownDomainID domain,
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

GURL GetContactURL(KnownDomainID tunnel_server,
                   base::span<const uint8_t> contact_id) {
  std::string contact_id_base64;
  base::Base64UrlEncode(
      std::string_view(reinterpret_cast<const char*>(contact_id.data()),
                       contact_id.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &contact_id_base64);
  GURL ret(std::string("wss://") + tunnelserver::DecodeDomain(tunnel_server) +
           "/cable/contact/" + contact_id_base64);
  DCHECK(ret.is_valid());
  return ret;
}

}  // namespace tunnelserver

namespace eid {

Components::Components() = default;
Components::~Components() = default;
Components::Components(const Components&) = default;

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

std::optional<CableEidArray> Decrypt(
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
    return std::nullopt;
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
    return std::nullopt;
  }

  uint16_t tunnel_server_domain;
  static_assert(EXTENT(plaintext) >= sizeof(tunnel_server_domain), "");
  memcpy(&tunnel_server_domain,
         &plaintext[EXTENT(plaintext) - sizeof(tunnel_server_domain)],
         sizeof(tunnel_server_domain));
  if (!tunnelserver::ToKnownDomainID(tunnel_server_domain)) {
    return std::nullopt;
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

  uint16_t tunnel_server_domain;
  memcpy(&tunnel_server_domain, &eid[1 + kNonceSize + sizeof(ret.routing_id)],
         sizeof(tunnel_server_domain));
  // |eid| has been checked by |Decrypt| so the tunnel server domain must be
  // valid.
  ret.tunnel_server_domain =
      *tunnelserver::ToKnownDomainID(tunnel_server_domain);

  return ret;
}

}  // namespace eid

namespace qr {

constexpr char kPrefix[] = "FIDO:/";

// DecompressPublicKey converts a compressed public key (from a scanned QR
// code) into a standard, uncompressed one.
static std::optional<std::array<uint8_t, device::kP256X962Length>>
DecompressPublicKey(base::span<const uint8_t> compressed_public_key) {
  if (compressed_public_key.size() !=
      device::cablev2::kCompressedPublicKeySize) {
    return std::nullopt;
  }

  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), compressed_public_key.data(),
                          compressed_public_key.size(), /*ctx=*/nullptr)) {
    return std::nullopt;
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
std::optional<Components> Parse(const std::string& qr_url) {
  if (qr_url.size() < sizeof(kPrefix) - 1 ||
      base::CompareCaseInsensitiveASCII(
          kPrefix, qr_url.substr(0, sizeof(kPrefix) - 1)) != 0) {
    return std::nullopt;
  }

  std::optional<std::vector<uint8_t>> qr_bytes =
      DigitsToBytes(qr_url.substr(sizeof(kPrefix) - 1));
  if (!qr_bytes) {
    return std::nullopt;
  }

  std::optional<cbor::Value> qr_contents = cbor::Reader::Read(*qr_bytes);
  if (!qr_contents || !qr_contents->is_map()) {
    return std::nullopt;
  }
  const cbor::Value::MapValue& qr_contents_map(qr_contents->GetMap());

  base::span<const uint8_t> values[2];
  for (size_t i = 0; i < std::size(values); i++) {
    const cbor::Value::MapValue::const_iterator it =
        qr_contents_map.find(cbor::Value(static_cast<int>(i)));
    if (it == qr_contents_map.end() || !it->second.is_bytestring()) {
      return std::nullopt;
    }
    values[i] = it->second.GetBytestring();
  }

  base::span<const uint8_t> compressed_public_key = values[0];
  base::span<const uint8_t> qr_secret = values[1];

  Components ret;
  if (qr_secret.size() != ret.secret.size()) {
    return std::nullopt;
  }
  base::ranges::copy(qr_secret, ret.secret.begin());

  std::optional<std::array<uint8_t, device::kP256X962Length>> peer_identity =
      DecompressPublicKey(compressed_public_key);
  if (!peer_identity) {
    FIDO_LOG(ERROR) << "Invalid compressed public key in QR data";
    return std::nullopt;
  }
  ret.peer_identity = *peer_identity;

  auto it = qr_contents_map.find(cbor::Value(2));
  if (it != qr_contents_map.end()) {
    if (!it->second.is_integer()) {
      return std::nullopt;
    }
    ret.num_known_domains = it->second.GetInteger();
  }

  it = qr_contents_map.find(cbor::Value(4));
  if (it != qr_contents_map.end()) {
    if (!it->second.is_bool()) {
      return std::nullopt;
    }
    ret.supports_linking = it->second.GetBool();
  }

  it = qr_contents_map.find(cbor::Value(5));
  if (it != qr_contents_map.end()) {
    if (!it->second.is_string()) {
      return std::nullopt;
    }
    ret.request_type = RequestTypeFromString(it->second.GetString());
  }

  return ret;
}

std::string Encode(base::span<const uint8_t, kQRKeySize> qr_key,
                   RequestType request_type) {
  cbor::Value::MapValue qr_contents;
  qr_contents.emplace(
      0, SeedToCompressedPublicKey(
             base::span<const uint8_t, device::cablev2::kQRSeedSize>(
                 qr_key.data(), device::cablev2::kQRSeedSize)));

  qr_contents.emplace(1, qr_key.subspan(device::cablev2::kQRSeedSize));

  qr_contents.emplace(
      2, static_cast<int64_t>(std::size(tunnelserver::kAssignedDomains)));

  qr_contents.emplace(3, static_cast<int64_t>(base::Time::Now().ToTimeT()));

  qr_contents.emplace(4, true);  // client supports storing linking information.

  qr_contents.emplace(5, RequestTypeToString(request_type));

  const std::optional<std::vector<uint8_t>> qr_data =
      cbor::Writer::Write(cbor::Value(std::move(qr_contents)));
  return std::string(kPrefix) + BytesToDigits(*qr_data);
}

// When converting between bytes and digits, chunks of 7 bytes are turned into
// 17 digits. See https://www.imperialviolet.org/2021/08/26/qrencoding.html.
constexpr size_t kChunkSize = 7;
constexpr size_t kChunkDigits = 17;

std::string BytesToDigits(base::span<const uint8_t> in) {
  std::string ret;
  ret.reserve(((in.size() + kChunkSize - 1) / kChunkSize) * kChunkDigits);

  while (in.size() >= kChunkSize) {
    uint64_t v = 0;
    static_assert(sizeof(v) >= kChunkSize, "");
    memcpy(&v, in.data(), kChunkSize);

    char digits[kChunkDigits + 1];
    static_assert(kChunkDigits == 17, "Need to change next line");
    CHECK_LT(snprintf(digits, sizeof(digits), "%017" PRIu64, v),
             static_cast<int>(sizeof(digits)));
    ret += digits;

    in = in.subspan(kChunkSize);
  }

  if (in.size()) {
    char format[16];
    // kPartialChunkDigits is the number of digits needed to encode each length
    // of trailing data from 6 bytes down to zero. I.e. it's 15, 13, 10, 8, 5,
    // 3, 0 written in hex.
    constexpr uint32_t kPartialChunkDigits = 0x0fda8530;
    CHECK_LT(snprintf(format, sizeof(format), "%%0%d" PRIu64,
                      15 & (kPartialChunkDigits >> (4 * in.size()))),
             static_cast<int>(sizeof(format)));

    uint64_t v = 0;
    CHECK_LE(in.size(), sizeof(v));
    memcpy(&v, in.data(), in.size());

    char digits[kChunkDigits + 1];
    CHECK_LT(snprintf(digits, sizeof(digits), format, v),
             static_cast<int>(sizeof(digits)));
    ret += digits;
  }

  return ret;
}

std::optional<std::vector<uint8_t>> DigitsToBytes(std::string_view in) {
  std::vector<uint8_t> ret;
  ret.reserve(((in.size() + kChunkDigits - 1) / kChunkDigits) * kChunkSize);

  while (in.size() >= kChunkDigits) {
    uint64_t v;
    if (!base::StringToUint64(in.substr(0, kChunkDigits), &v) ||
        v >> (kChunkSize * 8) != 0) {
      return std::nullopt;
    }
    const uint8_t* const v_bytes = reinterpret_cast<uint8_t*>(&v);
    ret.insert(ret.end(), v_bytes, v_bytes + kChunkSize);

    in = in.substr(kChunkDigits);
  }

  if (in.size()) {
    size_t remaining_bytes;
    switch (in.size()) {
      case 3:
        remaining_bytes = 1;
        break;
      case 5:
        remaining_bytes = 2;
        break;
      case 8:
        remaining_bytes = 3;
        break;
      case 10:
        remaining_bytes = 4;
        break;
      case 13:
        remaining_bytes = 5;
        break;
      case 15:
        remaining_bytes = 6;
        break;
      default:
        return std::nullopt;
    }

    uint64_t v;
    if (!base::StringToUint64(in, &v) || v >> (remaining_bytes * 8) != 0) {
      return std::nullopt;
    }

    const uint8_t* const v_bytes = reinterpret_cast<uint8_t*>(&v);
    ret.insert(ret.end(), v_bytes, v_bytes + remaining_bytes);
  }

  return ret;
}

}  // namespace qr

namespace sync {

uint32_t IDNow() {
  const base::Time now = base::Time::Now();
  time_t utc_time = now.ToTimeT();
  // The IDs, and thus Sync secret rotation, have a period of one day. These
  // are lazily updated by the phone and don't cause additional Sync uploads.
  utc_time /= 86400;
  // A uint32_t can span about 11 million years.
  return static_cast<uint32_t>(utc_time);
}

bool IDIsMoreThanNPeriodsOld(uint32_t candidate, unsigned periods) {
  const uint32_t now = IDNow();
  return candidate > now || (now - candidate) > periods;
}

}  // namespace sync

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

const char* RequestTypeToString(RequestType request_type) {
  return absl::visit(
      base::Overloaded{[](const FidoRequestType& request_type) {
                         switch (request_type) {
                           case FidoRequestType::kMakeCredential:
                             return "mc";
                           case FidoRequestType::kGetAssertion:
                             return "ga";
                             // If adding a value here, also update
                             // `RequestTypeFromString`.
                         }
                       },
                       [](const CredentialRequestType& request_type) {
                         switch (request_type) {
                           case CredentialRequestType::kPresentation:
                             return "dcp";
                             // If adding a value here, also update
                             // `RequestTypeFromString`.
                         }
                       }},
      request_type);
}

RequestType RequestTypeFromString(const std::string& s) {
  if (s == "mc") {
    return FidoRequestType::kMakeCredential;
  } else if (s == "dcp") {
    return CredentialRequestType::kPresentation;
  }
  // kGetAssertion is the default if the value is unknown too.
  return FidoRequestType::kGetAssertion;
}

bssl::UniquePtr<EC_KEY> IdentityKey(base::span<const uint8_t, 32> root_secret) {
  std::array<uint8_t, 32> seed;
  seed = device::cablev2::Derive<EXTENT(seed)>(
      root_secret, /*nonce=*/base::span<uint8_t>(),
      device::cablev2::DerivedValueType::kIdentityKeySeed);
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  return bssl::UniquePtr<EC_KEY>(
      EC_KEY_derive_from_secret(p256.get(), seed.data(), seed.size()));
}

bssl::UniquePtr<EC_KEY> ECKeyFromSeed(
    base::span<const uint8_t, kQRSeedSize> seed) {
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  return bssl::UniquePtr<EC_KEY>(
      EC_KEY_derive_from_secret(p256.get(), seed.data(), seed.size()));
}

std::optional<std::vector<uint8_t>> EncodePaddedCBORMap(
    cbor::Value::MapValue map) {
  // The number of padding bytes is a uint16_t, so the granularity cannot be
  // larger than that.
  static_assert(kPostHandshakeMsgPaddingGranularity > 0, "");
  static_assert(kPostHandshakeMsgPaddingGranularity - 1 <=
                std::numeric_limits<uint16_t>::max());
  // The granularity must also be a power of two.
  static_assert((kPostHandshakeMsgPaddingGranularity &
                 (kPostHandshakeMsgPaddingGranularity - 1)) == 0);

  std::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  if (!cbor_bytes) {
    return std::nullopt;
  }

  base::CheckedNumeric<size_t> padded_size_checked = cbor_bytes->size();
  padded_size_checked += sizeof(uint16_t);  // padding-length bytes
  padded_size_checked =
      (padded_size_checked + kPostHandshakeMsgPaddingGranularity - 1) &
      ~(kPostHandshakeMsgPaddingGranularity - 1);
  if (!padded_size_checked.IsValid()) {
    return std::nullopt;
  }

  const size_t padded_size = padded_size_checked.ValueOrDie();
  DCHECK_GE(padded_size, cbor_bytes->size() + sizeof(uint16_t));
  const size_t extra_bytes = padded_size - cbor_bytes->size();
  const size_t num_padding_bytes =
      extra_bytes - sizeof(uint16_t) /* length of padding length */;

  cbor_bytes->resize(padded_size);
  const uint16_t num_padding_bytes16 =
      base::checked_cast<uint16_t>(num_padding_bytes);
  memcpy(&cbor_bytes.value()[padded_size - sizeof(num_padding_bytes16)],
         &num_padding_bytes16, sizeof(num_padding_bytes16));

  return *cbor_bytes;
}

namespace {

// DecodePaddedCBORMap8 performs the actions of |DecodePaddedCBORMap| using the
// old padding format. We still support this format for backwards compatibility.
// See comment in |DecodePaddedCBORMap|.
//
// TODO(agl): remove support for this padding format. (Chromium started sending
// the new format with M99.)
std::optional<cbor::Value> DecodePaddedCBORMap8(
    const base::span<const uint8_t> input) {
  if (input.empty()) {
    return std::nullopt;
  }

  const size_t padding_length = input.back();
  if (padding_length + 1 > input.size()) {
    return std::nullopt;
  }
  auto unpadded_input = input.first(input.size() - padding_length - 1);

  std::optional<cbor::Value> payload = cbor::Reader::Read(unpadded_input);
  if (!payload || !payload->is_map()) {
    return std::nullopt;
  }

  return payload;
}

// DecodePaddedCBORMap16 performs the actions of |DecodePaddedCBORMap| using the
// new padding format. See comment in |DecodePaddedCBORMap|.
std::optional<cbor::Value> DecodePaddedCBORMap16(
    base::span<const uint8_t> input) {
  if (input.size() < sizeof(uint16_t)) {
    return std::nullopt;
  }

  uint16_t padding_length16;
  memcpy(&padding_length16, &input[input.size() - sizeof(padding_length16)],
         sizeof(padding_length16));
  const size_t padding_length = padding_length16;
  if (padding_length + sizeof(uint16_t) > input.size()) {
    return std::nullopt;
  }
  input = input.first(input.size() - padding_length - sizeof(uint16_t));

  std::optional<cbor::Value> payload = cbor::Reader::Read(input);
  if (!payload || !payload->is_map()) {
    return std::nullopt;
  }

  return payload;
}

}  // namespace

std::optional<cbor::Value> DecodePaddedCBORMap(
    base::span<const uint8_t> input) {
  // Two padding formats are currently in use. They are unambiguous so we try
  // each, new first. Eventually the old format can be removed once enough time
  // has passed since M99.
  std::optional<cbor::Value> result = DecodePaddedCBORMap16(input);
  if (!result) {
    result = DecodePaddedCBORMap8(input);
  }
  if (!result) {
    FIDO_LOG(DEBUG) << "Invalid padding in caBLE handshake message";
  }
  return result;
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
  static_assert(std::has_single_bit(kPaddingGranularity),
                "padding must be a power of two");

  // Padding consists of a some number of zero bytes appended to the message
  // and the final byte in the message is the number of zeros.
  base::CheckedNumeric<size_t> padded_size_checked = message_to_encrypt->size();
  padded_size_checked += 1;  // padding-length byte.
  padded_size_checked = (padded_size_checked + kPaddingGranularity - 1) &
                        ~(kPaddingGranularity - 1);
  if (!padded_size_checked.IsValid()) {
    NOTREACHED();
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

  base::span<const uint8_t> additional_data;
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

  base::span<const uint8_t> additional_data;
  std::optional<std::vector<uint8_t>> plaintext =
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
    std::optional<base::span<const uint8_t, 32>> psk,
    std::optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
    std::optional<base::span<const uint8_t, kQRSeedSize>> identity_seed)
    : local_identity_(identity_seed ? ECKeyFromSeed(*identity_seed) : nullptr) {
  DCHECK(peer_identity.has_value() ^ static_cast<bool>(local_identity_));
  if (peer_identity) {
    peer_identity_ =
        fido_parsing_utils::Materialize<kP256X962Length>(*peer_identity);
  }
  if (psk) {
    psk_ = fido_parsing_utils::Materialize(*psk);
  }
}

HandshakeInitiator::~HandshakeInitiator() = default;

std::vector<uint8_t> HandshakeInitiator::BuildInitialMessage() {
  uint8_t prologue[1];

  if (!psk_.has_value()) {
    noise_.Init(Noise::HandshakeType::kNK);
    prologue[0] = 0;
    noise_.MixHash(prologue);
    noise_.MixHash(*peer_identity_);
  } else if (peer_identity_) {
    noise_.Init(Noise::HandshakeType::kNKpsk0);
    prologue[0] = 0;
    noise_.MixHash(prologue);
    noise_.MixHash(*peer_identity_);
    noise_.MixKeyAndHash(*psk_);
  } else {
    noise_.Init(Noise::HandshakeType::kKNpsk0);
    prologue[0] = 1;
    noise_.MixHash(prologue);
    noise_.MixHashPoint(EC_KEY_get0_public_key(local_identity_.get()));
    noise_.MixKeyAndHash(*psk_);
  }

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
  if (response.size() != kResponseSize) {
    FIDO_LOG(DEBUG) << "Handshake response wrong size (" << response.size()
                    << " bytes)";
    return std::nullopt;
  }
  auto peer_point_bytes = response.first(kP256X962Length);
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
    return std::nullopt;
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
      return std::nullopt;
    }
    noise_.MixKey(shared_key_se);
  }

  auto plaintext = noise_.DecryptAndHash(ciphertext);
  if (!plaintext || !plaintext->empty()) {
    FIDO_LOG(DEBUG) << "Invalid caBLE handshake message";
    return std::nullopt;
  }

  auto [write_key, read_key] = noise_.traffic_keys();
  return std::make_pair(std::make_unique<cablev2::Crypter>(read_key, write_key),
                        noise_.handshake_hash());
}

HandshakeResult RespondToHandshake(
    std::optional<base::span<const uint8_t, 32>> psk,
    bssl::UniquePtr<EC_KEY> identity,
    std::optional<base::span<const uint8_t, kP256X962Length>> peer_identity,
    base::span<const uint8_t> in,
    std::vector<uint8_t>* out_response) {
  DCHECK(peer_identity.has_value() ^ static_cast<bool>(identity));

  if (in.size() < kP256X962Length) {
    FIDO_LOG(DEBUG) << "Handshake truncated (" << in.size() << " bytes)";
    return std::nullopt;
  }
  auto peer_point_bytes = in.first(kP256X962Length);
  auto ciphertext = in.subspan(kP256X962Length);

  Noise noise;
  uint8_t prologue[1];
  if (!psk.has_value()) {
    noise.Init(device::Noise::HandshakeType::kNK);
    prologue[0] = 0;
    noise.MixHash(prologue);
    noise.MixHashPoint(EC_KEY_get0_public_key(identity.get()));
  } else if (identity) {
    noise.Init(device::Noise::HandshakeType::kNKpsk0);
    prologue[0] = 0;
    noise.MixHash(prologue);
    noise.MixHashPoint(EC_KEY_get0_public_key(identity.get()));
    noise.MixKeyAndHash(*psk);
  } else {
    noise.Init(device::Noise::HandshakeType::kKNpsk0);
    prologue[0] = 1;
    noise.MixHash(prologue);
    noise.MixHash(*peer_identity);
    noise.MixKeyAndHash(*psk);
  }

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
    return std::nullopt;
  }

  if (identity) {
    uint8_t es_key[32];
    if (ECDH_compute_key(es_key, sizeof(es_key), peer_point.get(),
                         identity.get(),
                         /*kdf=*/nullptr) != sizeof(es_key)) {
      return std::nullopt;
    }
    noise.MixKey(es_key);
  }

  auto plaintext = noise.DecryptAndHash(ciphertext);
  if (!plaintext || !plaintext->empty()) {
    FIDO_LOG(DEBUG) << "Failed to decrypt handshake ciphertext.";
    return std::nullopt;
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
    return std::nullopt;
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
      return std::nullopt;
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

  auto [read_key, write_key] = noise.traffic_keys();
  return std::make_pair(std::make_unique<cablev2::Crypter>(read_key, write_key),
                        noise.handshake_hash());
}

bool VerifyPairingSignature(
    base::span<const uint8_t, kQRSeedSize> identity_seed,
    base::span<const uint8_t, kP256X962Length> peer_public_key_x962,
    base::span<const uint8_t, std::tuple_size<HandshakeHash>::value>
        handshake_hash,
    base::span<const uint8_t> signature) {
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> unused(EC_POINT_new(p256.get()));
  if (EC_POINT_oct2point(p256.get(), unused.get(), peer_public_key_x962.data(),
                         peer_public_key_x962.size(),
                         /*ctx=*/nullptr) != 1) {
    return false;
  }

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
