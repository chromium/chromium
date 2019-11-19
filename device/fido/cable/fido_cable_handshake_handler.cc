// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/fido_cable_handshake_handler.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/aead.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "device/fido/cable/fido_cable_device.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/hkdf.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {

namespace {

// Length of CBOR encoded authenticator hello message concatenated with
// 16 byte message authentication code.
constexpr size_t kCableAuthenticatorHandshakeMessageSize = 66;

// Length of CBOR encoded client hello message concatenated with 16 byte message
// authenticator code.
constexpr size_t kClientHelloMessageSize = 58;

constexpr size_t kCableHandshakeMacMessageSize = 16;

base::Optional<std::array<uint8_t, kClientHelloMessageSize>>
ConstructHandshakeMessage(base::StringPiece handshake_key,
                          base::span<const uint8_t, 16> client_random_nonce) {
  cbor::Value::MapValue map;
  map.emplace(0, kCableClientHelloMessage);
  map.emplace(1, client_random_nonce);
  auto client_hello = cbor::Writer::Write(cbor::Value(std::move(map)));
  DCHECK(client_hello);

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key))
    return base::nullopt;

  std::array<uint8_t, 32> client_hello_mac;
  if (!hmac.Sign(fido_parsing_utils::ConvertToStringPiece(*client_hello),
                 client_hello_mac.data(), client_hello_mac.size())) {
    return base::nullopt;
  }

  DCHECK_EQ(kClientHelloMessageSize,
            client_hello->size() + kCableHandshakeMacMessageSize);
  std::array<uint8_t, kClientHelloMessageSize> handshake_message;
  std::copy(client_hello->begin(), client_hello->end(),
            handshake_message.begin());
  std::copy(client_hello_mac.begin(),
            client_hello_mac.begin() + kCableHandshakeMacMessageSize,
            handshake_message.begin() + client_hello->size());

  return handshake_message;
}

}  // namespace

FidoCableHandshakeHandler::~FidoCableHandshakeHandler() {}

FidoCableV1HandshakeHandler::FidoCableV1HandshakeHandler(
    FidoCableDevice* cable_device,
    base::span<const uint8_t, 8> nonce,
    base::span<const uint8_t, 32> session_pre_key)
    : cable_device_(cable_device),
      nonce_(fido_parsing_utils::Materialize(nonce)),
      session_pre_key_(fido_parsing_utils::Materialize(session_pre_key)),
      handshake_key_(crypto::HkdfSha256(
          fido_parsing_utils::ConvertToStringPiece(session_pre_key_),
          fido_parsing_utils::ConvertToStringPiece(nonce_),
          kCableHandshakeKeyInfo,
          /*derived_key_size=*/32)) {
  crypto::RandBytes(client_session_random_.data(),
                    client_session_random_.size());
}

FidoCableV1HandshakeHandler::~FidoCableV1HandshakeHandler() = default;

void FidoCableV1HandshakeHandler::InitiateCableHandshake(
    FidoDevice::DeviceCallback callback) {
  auto handshake_message =
      ConstructHandshakeMessage(handshake_key_, client_session_random_);
  if (!handshake_message) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt));
    return;
  }

  FIDO_LOG(DEBUG) << "Sending the caBLE handshake message";
  cable_device_->SendHandshakeMessage(
      fido_parsing_utils::Materialize(*handshake_message), std::move(callback));
}

bool FidoCableV1HandshakeHandler::ValidateAuthenticatorHandshakeMessage(
    base::span<const uint8_t> response) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  if (!hmac.Init(handshake_key_))
    return false;

  if (response.size() != kCableAuthenticatorHandshakeMessageSize) {
    return false;
  }

  const auto authenticator_hello = response.first(
      kCableAuthenticatorHandshakeMessageSize - kCableHandshakeMacMessageSize);
  if (!hmac.VerifyTruncated(
          fido_parsing_utils::ConvertToStringPiece(authenticator_hello),
          fido_parsing_utils::ConvertToStringPiece(
              response.subspan(authenticator_hello.size())))) {
    return false;
  }

  const auto authenticator_hello_cbor = cbor::Reader::Read(authenticator_hello);
  if (!authenticator_hello_cbor || !authenticator_hello_cbor->is_map() ||
      authenticator_hello_cbor->GetMap().size() != 2) {
    return false;
  }

  const auto authenticator_hello_msg =
      authenticator_hello_cbor->GetMap().find(cbor::Value(0));
  if (authenticator_hello_msg == authenticator_hello_cbor->GetMap().end() ||
      !authenticator_hello_msg->second.is_string() ||
      authenticator_hello_msg->second.GetString() !=
          kCableAuthenticatorHelloMessage) {
    return false;
  }

  const auto authenticator_random_nonce =
      authenticator_hello_cbor->GetMap().find(cbor::Value(1));
  if (authenticator_random_nonce == authenticator_hello_cbor->GetMap().end() ||
      !authenticator_random_nonce->second.is_bytestring() ||
      authenticator_random_nonce->second.GetBytestring().size() != 16) {
    return false;
  }

  cable_device_->SetV1EncryptionData(
      base::make_span<32>(
          GetEncryptionKeyAfterSuccessfulHandshake(base::make_span<16>(
              authenticator_random_nonce->second.GetBytestring()))),
      nonce_);

  return true;
}

std::vector<uint8_t>
FidoCableV1HandshakeHandler::GetEncryptionKeyAfterSuccessfulHandshake(
    base::span<const uint8_t, 16> authenticator_random_nonce) const {
  std::vector<uint8_t> nonce_message;
  fido_parsing_utils::Append(&nonce_message, nonce_);
  fido_parsing_utils::Append(&nonce_message, client_session_random_);
  fido_parsing_utils::Append(&nonce_message, authenticator_random_nonce);
  return crypto::HkdfSha256(session_pre_key_, crypto::SHA256Hash(nonce_message),
                            kCableDeviceEncryptionKeyInfo,
                            /*derived_key_length=*/32);
}

// kP256PointSize is the number of bytes in an X9.62 encoding of a P-256 point.
static constexpr size_t kP256PointSize = 65;

FidoCableV2HandshakeHandler::FidoCableV2HandshakeHandler(
    FidoCableDevice* cable_device,
    base::span<const uint8_t, 32> psk_gen_key,
    base::span<const uint8_t, 8> nonce,
    base::span<const uint8_t, kCableEphemeralIdSize> eid,
    base::Optional<base::span<const uint8_t, kP256PointSize>> peer_identity,
    base::RepeatingCallback<void(std::unique_ptr<CableDiscoveryData>)>
        pairing_callback)
    : cable_device_(cable_device),
      eid_(fido_parsing_utils::Materialize(eid)),
      pairing_callback_(std::move(pairing_callback)) {
  HKDF(psk_.data(), psk_.size(), EVP_sha256(), psk_gen_key.data(),
       psk_gen_key.size(), /*salt=*/nonce.data(), nonce.size(),
       /*info=*/nullptr, 0);
  if (peer_identity) {
    peer_identity_ = fido_parsing_utils::Materialize(*peer_identity);
  }
}

FidoCableV2HandshakeHandler::~FidoCableV2HandshakeHandler() {}

namespace {

// HKDF2 implements the functions with the same name from Noise[1], specialized
// to the case where |num_outputs| is two.
//
// [1] https://www.noiseprotocol.org/noise.html#hash-functions
std::tuple<std::array<uint8_t, 32>, std::array<uint8_t, 32>> HKDF2(
    base::span<const uint8_t, 32> ck,
    base::span<const uint8_t> ikm) {
  uint8_t output[32 * 2];
  HKDF(output, sizeof(output), EVP_sha256(), ikm.data(), ikm.size(), ck.data(),
       ck.size(), /*info=*/nullptr, 0);

  std::array<uint8_t, 32> a, b;
  memcpy(a.data(), &output[0], 32);
  memcpy(b.data(), &output[32], 32);

  return std::make_tuple(a, b);
}

// HKDF3 implements the functions with the same name from Noise[1], specialized
// to the case where |num_outputs| is three.
//
// [1] https://www.noiseprotocol.org/noise.html#hash-functions
std::tuple<std::array<uint8_t, 32>,
           std::array<uint8_t, 32>,
           std::array<uint8_t, 32>>
HKDF3(base::span<const uint8_t, 32> ck, base::span<const uint8_t> ikm) {
  uint8_t output[32 * 3];
  HKDF(output, sizeof(output), EVP_sha256(), ikm.data(), ikm.size(), ck.data(),
       ck.size(), /*info=*/nullptr, 0);

  std::array<uint8_t, 32> a, b, c;
  memcpy(a.data(), &output[0], 32);
  memcpy(b.data(), &output[32], 32);
  memcpy(c.data(), &output[64], 32);

  return std::make_tuple(a, b, c);
}

template <size_t N>
bool CopyBytestring(std::array<uint8_t, N>* out,
                    const cbor::Value::MapValue& map,
                    int key) {
  const auto it = map.find(cbor::Value(key));
  if (it == map.end() || !it->second.is_bytestring()) {
    return false;
  }
  const std::vector<uint8_t> bytestring = it->second.GetBytestring();
  return fido_parsing_utils::ExtractArray(bytestring, /*pos=*/0, out);
}

}  // namespace

void FidoCableV2HandshakeHandler::InitiateCableHandshake(
    FidoDevice::DeviceCallback callback) {
  // See https://www.noiseprotocol.org/noise.html#the-handshakestate-object
  static const char kNNProtocolName[] = "Noise_NNpsk0_P256_AESGCM_SHA256";
  static const char kNKProtocolName[] = "Noise_NKpsk0_P256_AESGCM_SHA256";
  static_assert(sizeof(kNKProtocolName) == sizeof(kNNProtocolName),
                "protocol names are different lengths");
  static_assert(sizeof(kNNProtocolName) == crypto::kSHA256Length,
                "name may need padding if not HASHLEN bytes long");
  static_assert(
      std::tuple_size<decltype(chaining_key_)>::value == crypto::kSHA256Length,
      "chaining_key_ is wrong size");
  static_assert(std::tuple_size<decltype(h_)>::value == crypto::kSHA256Length,
                "h_ is wrong size");
  if (peer_identity_) {
    memcpy(chaining_key_.data(), kNKProtocolName, sizeof(kNKProtocolName));
  } else {
    memcpy(chaining_key_.data(), kNNProtocolName, sizeof(kNNProtocolName));
  }
  h_ = chaining_key_;

  if (peer_identity_) {
    static const uint8_t kPrologue[] = "caBLE handshake";
    MixHash(kPrologue);
  } else {
    static const uint8_t kPrologue[] = "caBLE QR code handshake";
    MixHash(kPrologue);
  }

  MixKeyAndHash(psk_);
  ephemeral_key_.reset(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key_.get());
  CHECK(EC_KEY_generate_key(ephemeral_key_.get()));
  uint8_t ephemeral_key_public_bytes[kP256PointSize];
  CHECK_EQ(sizeof(ephemeral_key_public_bytes),
           EC_POINT_point2oct(
               group, EC_KEY_get0_public_key(ephemeral_key_.get()),
               POINT_CONVERSION_UNCOMPRESSED, ephemeral_key_public_bytes,
               sizeof(ephemeral_key_public_bytes), /*ctx=*/nullptr));
  MixHash(ephemeral_key_public_bytes);
  MixKey(ephemeral_key_public_bytes);

  if (peer_identity_) {
    // If we know the identity of the peer from a previous interaction, NKpsk0
    // is performed to ensure that other browsers, which may also know the PSK,
    // cannot impersonate the authenticator.
    bssl::UniquePtr<EC_POINT> peer_identity_point(EC_POINT_new(group));
    uint8_t es_key[32];
    if (!EC_POINT_oct2point(group, peer_identity_point.get(),
                            peer_identity_->data(), peer_identity_->size(),
                            /*ctx=*/nullptr) ||
        !ECDH_compute_key(es_key, sizeof(es_key), peer_identity_point.get(),
                          ephemeral_key_.get(), /*kdf=*/nullptr)) {
      FIDO_LOG(DEBUG) << "Dropping handshake because peer identity is invalid";
      return;
    }
    MixKey(es_key);
  }

  std::vector<uint8_t> ciphertext = Encrypt(base::span<const uint8_t>());
  MixHash(ciphertext);

  std::vector<uint8_t> handshake_message;
  handshake_message.reserve(eid_.size() + sizeof(ephemeral_key_public_bytes) +
                            ciphertext.size());
  handshake_message.insert(handshake_message.end(), eid_.begin(), eid_.end());
  handshake_message.insert(
      handshake_message.end(), ephemeral_key_public_bytes,
      ephemeral_key_public_bytes + sizeof(ephemeral_key_public_bytes));
  handshake_message.insert(handshake_message.end(), ciphertext.begin(),
                           ciphertext.end());

  cable_device_->SendHandshakeMessage(std::move(handshake_message),
                                      std::move(callback));
}

bool FidoCableV2HandshakeHandler::ValidateAuthenticatorHandshakeMessage(
    base::span<const uint8_t> response) {
  if (response.size() < kP256PointSize) {
    return false;
  }
  auto peer_point_bytes = response.subspan(0, kP256PointSize);
  auto ciphertext = response.subspan(kP256PointSize);

  bssl::UniquePtr<EC_POINT> peer_point(
      EC_POINT_new(EC_KEY_get0_group(ephemeral_key_.get())));
  uint8_t shared_key[32];
  const EC_GROUP* group = EC_KEY_get0_group(ephemeral_key_.get());
  if (!EC_POINT_oct2point(group, peer_point.get(), peer_point_bytes.data(),
                          peer_point_bytes.size(), /*ctx=*/nullptr) ||
      !ECDH_compute_key(shared_key, sizeof(shared_key), peer_point.get(),
                        ephemeral_key_.get(), /*kdf=*/nullptr)) {
    return false;
  }

  MixHash(peer_point_bytes);
  MixKey(peer_point_bytes);
  MixKey(shared_key);

  auto plaintext = Decrypt(ciphertext);
  if (!plaintext || plaintext->empty() != peer_identity_.has_value()) {
    FIDO_LOG(DEBUG) << "Invalid caBLE handshake message";
    return false;
  }

  if (!peer_identity_) {
    // Handshakes without a peer identity (i.e. NNpsk0 handshakes setup from a
    // QR code) send a padded message in the reply. This message can,
    // optionally, contain CBOR-encoded, long-term pairing information.
    const size_t padding_length = (*plaintext)[plaintext->size() - 1];
    if (padding_length + 1 > plaintext->size()) {
      FIDO_LOG(DEBUG) << "Invalid padding in caBLE handshake message";
      return false;
    }
    plaintext->resize(plaintext->size() - padding_length - 1);

    if (!plaintext->empty()) {
      base::Optional<cbor::Value> pairing = cbor::Reader::Read(*plaintext);
      if (!pairing || !pairing->is_map()) {
        FIDO_LOG(DEBUG) << "CBOR parse failure in caBLE handshake message";
        return false;
      }

      auto future_discovery = std::make_unique<CableDiscoveryData>();
      future_discovery->version = CableDiscoveryData::Version::V2;
      future_discovery->v2.emplace();
      future_discovery->v2->peer_identity.emplace();

      const cbor::Value::MapValue& pairing_map(pairing->GetMap());
      const auto name_it = pairing_map.find(cbor::Value(4));
      if (!CopyBytestring(&future_discovery->v2->eid_gen_key, pairing_map, 1) ||
          !CopyBytestring(&future_discovery->v2->psk_gen_key, pairing_map, 2) ||
          !CopyBytestring(&future_discovery->v2->peer_identity.value(),
                          pairing_map, 3) ||
          name_it == pairing_map.end() || !name_it->second.is_string() ||
          !EC_POINT_oct2point(group, peer_point.get(),
                              future_discovery->v2->peer_identity->data(),
                              future_discovery->v2->peer_identity->size(),
                              /*ctx=*/nullptr)) {
        FIDO_LOG(DEBUG) << "CBOR structure error in caBLE handshake message";
        return false;
      }

      future_discovery->v2->peer_name = name_it->second.GetString();
      pairing_callback_.Run(std::move(future_discovery));
    }
  }

  // Here the spec says to do MixHash(ciphertext), but there are no more
  // handshake messages so that's moot.
  // MixHash(ciphertext);

  std::array<uint8_t, 32> read_key, write_key;
  std::tie(write_key, read_key) =
      HKDF2(chaining_key_, base::span<const uint8_t>());
  cable_device_->SetV2EncryptionData(read_key, write_key);

  return true;
}

void FidoCableV2HandshakeHandler::MixHash(base::span<const uint8_t> in) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, h_.data(), h_.size());
  SHA256_Update(&ctx, in.data(), in.size());
  SHA256_Final(h_.data(), &ctx);
}

void FidoCableV2HandshakeHandler::MixKey(base::span<const uint8_t> ikm) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  std::array<uint8_t, 32> temp_k;
  std::tie(chaining_key_, temp_k) = HKDF2(chaining_key_, ikm);
  InitializeKey(temp_k);
}

void FidoCableV2HandshakeHandler::MixKeyAndHash(base::span<const uint8_t> ikm) {
  // See https://www.noiseprotocol.org/noise.html#the-symmetricstate-object
  std::array<uint8_t, 32> temp_h, temp_k;
  std::tie(chaining_key_, temp_h, temp_k) = HKDF3(chaining_key_, ikm);
  MixHash(temp_h);
  InitializeKey(temp_k);
}

void FidoCableV2HandshakeHandler::InitializeKey(
    base::span<const uint8_t, 32> key) {
  // See https://www.noiseprotocol.org/noise.html#the-cipherstate-object
  DCHECK_EQ(symmetric_key_.size(), key.size());
  memcpy(symmetric_key_.data(), key.data(), symmetric_key_.size());
  symmetric_nonce_ = 0;
}

std::vector<uint8_t> FidoCableV2HandshakeHandler::Encrypt(
    base::span<const uint8_t> plaintext) {
  uint8_t nonce[12] = {0};
  memcpy(nonce, &symmetric_nonce_, sizeof(symmetric_nonce_));
  symmetric_nonce_++;

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(symmetric_key_);
  return aead.Seal(base::span<const uint8_t>(), nonce, h_);
}

base::Optional<std::vector<uint8_t>> FidoCableV2HandshakeHandler::Decrypt(
    base::span<const uint8_t> ciphertext) {
  uint8_t nonce[12] = {0};
  memcpy(nonce, &symmetric_nonce_, sizeof(symmetric_nonce_));
  symmetric_nonce_++;

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(symmetric_key_);
  return aead.Open(ciphertext, nonce, h_);
}

}  // namespace device
