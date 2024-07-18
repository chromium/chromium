// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/cable/v2_handshake.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "crypto/random.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "url/gurl.h"

namespace device {
namespace cablev2 {

namespace {

TEST(CableV2Encoding, TunnelServerURLs) {
  uint8_t tunnel_id[16] = {0};
  // Tunnel ID zero should map to Google's tunnel server.
  const tunnelserver::KnownDomainID kGoogleDomain(0);
  const GURL url = tunnelserver::GetNewTunnelURL(kGoogleDomain, tunnel_id);

  EXPECT_TRUE(base::Contains(url.spec(), "//cable.ua5v.com/")) << url;

  // The hash function shouldn't change across releases, so test a hashed
  // domain.
  const tunnelserver::KnownDomainID kHashedDomain(266);
  const GURL hashed_url =
      tunnelserver::GetNewTunnelURL(kHashedDomain, tunnel_id);

  EXPECT_TRUE(base::Contains(hashed_url.spec(), "//cable.wufkweyy3uaxb.com/"))
      << hashed_url;
}

TEST(CableV2Encoding, EIDToFromComponents) {
  eid::Components components;
  components.tunnel_server_domain = tunnelserver::KnownDomainID(0x0102);
  components.routing_id = {9, 10, 11};
  crypto::RandBytes(components.nonce);

  const CableEidArray eid = eid::FromComponents(components);
  const eid::Components components2 = eid::ToComponents(eid);

  EXPECT_EQ(components.routing_id, components2.routing_id);
  EXPECT_EQ(components.tunnel_server_domain, components2.tunnel_server_domain);
  EXPECT_EQ(components.nonce, components2.nonce);
}

TEST(CableV2Encoding, EIDEncrypt) {
  eid::Components components;
  components.tunnel_server_domain = tunnelserver::KnownDomainID(0x0102);
  components.routing_id = {9, 10, 11};
  crypto::RandBytes(components.nonce);
  const CableEidArray eid = eid::FromComponents(components);

  uint8_t key[kEIDKeySize];
  crypto::RandBytes(key);
  std::array<uint8_t, kAdvertSize> advert = eid::Encrypt(eid, key);

  const std::optional<CableEidArray> eid2 = eid::Decrypt(advert, key);
  ASSERT_TRUE(eid2.has_value());
  EXPECT_TRUE(memcmp(eid.data(), eid2->data(), eid.size()) == 0);

  advert[0] ^= 1;
  EXPECT_FALSE(eid::Decrypt(advert, key).has_value());

  // Unknown tunnel server domains should not decrypt.
  components.tunnel_server_domain = tunnelserver::KnownDomainID(255);
  const CableEidArray eid3 = eid::FromComponents(components);
  std::array<uint8_t, kAdvertSize> invalid_advert = eid::Encrypt(eid3, key);
  EXPECT_FALSE(eid::Decrypt(invalid_advert, key).has_value());
}

TEST(CableV2Encoding, QRs) {
  std::array<uint8_t, kQRKeySize> qr_key;
  crypto::RandBytes(qr_key);
  std::string url = qr::Encode(qr_key, FidoRequestType::kMakeCredential);
  const std::optional<qr::Components> decoded = qr::Parse(url);
  ASSERT_TRUE(decoded.has_value()) << url;
  static_assert(EXTENT(qr_key) >= EXTENT(decoded->secret), "");
  EXPECT_EQ(memcmp(decoded->secret.data(),
                   &qr_key[qr_key.size() - decoded->secret.size()],
                   decoded->secret.size()),
            0);
  // There are two registered domains at the time of writing the test. That
  // number should only grow over time.
  EXPECT_GE(decoded->num_known_domains, 2u);

  // Chromium always sets this flag.
  EXPECT_TRUE(decoded->supports_linking.value_or(false));

  EXPECT_EQ(decoded->request_type,
            RequestType(FidoRequestType::kMakeCredential));

  url[0] ^= 4;
  EXPECT_FALSE(qr::Parse(url));
  EXPECT_FALSE(qr::Parse("nonsense"));
}

TEST(CableV2Encoding, KnownQRs) {
  static const uint8_t kCompressedPoint[] = {
      0x03, 0x36, 0x4C, 0x15, 0xEE, 0xC3, 0x43, 0x31, 0xD2, 0x86, 0x57,
      0x57, 0x42, 0x1D, 0x49, 0x7E, 0x56, 0x9E, 0x1E, 0xBA, 0x6C, 0xFF,
      0x9A, 0x69, 0xD3, 0x2E, 0x90, 0xF1, 0x9E, 0x7F, 0x6F, 0xD1, 0x5E,
  };
  static const uint8_t kQRSecret[16] = {0};

  const struct {
    std::function<void(cbor::Value::MapValue* m)> build;
    bool is_valid;
    int64_t num_known_domains;
    std::optional<bool> supports_linking;
    RequestType request_type;
  } kTests[] = {
      {
          // Basic, but valid, QR.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
          },
          /* is_valid= */ true,
          /* num_known_domains= */ 0,
          /* supports_linking= */ std::nullopt,
          /* request_type= */ FidoRequestType::kGetAssertion,
      },
      {
          // QR with an invalid compressed point.
          [](cbor::Value::MapValue* m) {
            uint8_t invalid_point[sizeof(kCompressedPoint)];
            memcpy(invalid_point, kCompressedPoint, sizeof(invalid_point));
            invalid_point[sizeof(invalid_point) - 1] ^= 3;
            m->emplace(0, base::span(invalid_point));
            m->emplace(1, base::span(kQRSecret));
          },
          /* is_valid= */ false,
      },
      {
          // Incorrect structure.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, 42);  // invalid: not a bytestring
            m->emplace(1, base::span(kQRSecret));
          },
          /* is_valid= */ false,
      },
      {
          // Valid, contains number of known domains.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(2, 4567);
          },
          /* is_valid= */ true,
          /* num_known_domains= */ 4567,
          /* supports_linking= */ std::nullopt,
          /* request_type= */ FidoRequestType::kGetAssertion,
      },
      {
          // Incorrect structure.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(2, "foo");  // invalid: not a number
          },
          /* is_valid= */ false,
      },
      {
          // Supports linking.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(4, true);
          },
          /* is_valid= */ true,
          /* num_known_domains= */ 0,
          /* supports_linking= */ true,
          /* request_type= */ FidoRequestType::kGetAssertion,
      },
      {
          // Explicitly does not support linking.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(4, false);
          },
          /* is_valid= */ true,
          /* num_known_domains= */ 0,
          /* supports_linking= */ false,
          /* request_type= */ FidoRequestType::kGetAssertion,
      },
      {
          // Incorrect structure.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(4, "foo");  // invalid: not a boolean
          },
          /* is_valid= */ false,
      },
      {
          // Includes request type.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(5, "ga");
          },
          /* is_valid= */ true,
          /* num_known_domains= */ 0,
          /* supports_linking= */ std::nullopt,
          /* request_type= */ FidoRequestType::kGetAssertion,
      },
      {
          // Other request type.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(5, "mc");
          },
          /* is_valid= */ true,
          /* num_known_domains= */ 0,
          /* supports_linking= */ std::nullopt,
          /* request_type= */ FidoRequestType::kMakeCredential,
      },
      {
          // Unknown request type.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(5, "XX");  // unknown values are mapped to "ga"
          },
          /* is_valid= */ true,
          /* num_known_domains= */ 0,
          /* supports_linking= */ std::nullopt,
          /* request_type= */ FidoRequestType::kGetAssertion,
      },
      {
          // Incorrect structure.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(5, 42);  // invalid: not a string
          },
          /* is_valid= */ false,
      },
      {
          // Contains an unknown key.
          [](cbor::Value::MapValue* m) {
            m->emplace(0, base::span(kCompressedPoint));
            m->emplace(1, base::span(kQRSecret));
            m->emplace(1000, 42);  // unknown keys are ignored.
          },
          /* is_valid= */ true,
          /* num_known_domains= */ 0,
          /* supports_linking= */ std::nullopt,
          /* request_type= */ FidoRequestType::kGetAssertion,
      },
  };

  int test_num = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num);
    test_num++;

    cbor::Value::MapValue map;
    test.build(&map);
    const std::optional<std::vector<uint8_t>> qr_data =
        cbor::Writer::Write(cbor::Value(std::move(map)));
    const std::string qr = std::string("FIDO:/") + qr::BytesToDigits(*qr_data);
    const std::optional<qr::Components> decoded = qr::Parse(qr);

    EXPECT_EQ(decoded.has_value(), test.is_valid);
    if (!decoded.has_value() || !test.is_valid) {
      continue;
    }

    EXPECT_EQ(decoded->num_known_domains, test.num_known_domains);
    EXPECT_EQ(decoded->supports_linking, test.supports_linking);
    EXPECT_EQ(decoded->request_type, test.request_type);
  }
}

TEST(CableV2Encoding, RequestTypeToString) {
  for (const auto type :
       {FidoRequestType::kMakeCredential, FidoRequestType::kGetAssertion}) {
    EXPECT_EQ(RequestType(type),
              RequestTypeFromString(RequestTypeToString(type)));
  }
  EXPECT_EQ(RequestType(CredentialRequestType::kPresentation),
            RequestTypeFromString(
                RequestTypeToString(CredentialRequestType::kPresentation)));

  EXPECT_EQ(RequestType(FidoRequestType::kGetAssertion),
            RequestTypeFromString("nonsense"));
  EXPECT_EQ(RequestType(FidoRequestType::kGetAssertion),
            RequestTypeFromString(""));
}

TEST(CableV2Encoding, PaddedCBOR) {
  cbor::Value::MapValue map1;
  std::optional<std::vector<uint8_t>> encoded =
      EncodePaddedCBORMap(std::move(map1));
  ASSERT_TRUE(encoded);
  EXPECT_EQ(kPostHandshakeMsgPaddingGranularity, encoded->size());

  std::optional<cbor::Value> decoded = DecodePaddedCBORMap(*encoded);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(0u, decoded->GetMap().size());

  cbor::Value::MapValue map2;
  uint8_t blob[kPostHandshakeMsgPaddingGranularity] = {0};
  map2.emplace(1, base::span<const uint8_t>(blob, sizeof(blob)));
  encoded = EncodePaddedCBORMap(std::move(map2));
  ASSERT_TRUE(encoded);
  EXPECT_EQ(kPostHandshakeMsgPaddingGranularity * 2, encoded->size());

  decoded = DecodePaddedCBORMap(*encoded);
  ASSERT_TRUE(decoded);
  EXPECT_EQ(1u, decoded->GetMap().size());
}

// EncodePaddedCBORMapOld is the old padding function that used to be used.
// We should still be compatible with it until M99 has been out in the world
// for long enough.
std::optional<std::vector<uint8_t>> EncodePaddedCBORMapOld(
    cbor::Value::MapValue map) {
  std::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  if (!cbor_bytes) {
    return std::nullopt;
  }

  base::CheckedNumeric<size_t> padded_size_checked = cbor_bytes->size();
  padded_size_checked += 1;  // padding-length byte
  padded_size_checked = (padded_size_checked + 255) & ~255;
  if (!padded_size_checked.IsValid()) {
    return std::nullopt;
  }

  const size_t padded_size = padded_size_checked.ValueOrDie();
  DCHECK_GT(padded_size, cbor_bytes->size());
  const size_t extra_padding = padded_size - cbor_bytes->size();

  cbor_bytes->resize(padded_size);
  DCHECK_LE(extra_padding, 256u);
  cbor_bytes->at(padded_size - 1) = static_cast<uint8_t>(extra_padding - 1);

  return *cbor_bytes;
}

TEST(CableV2Encoding, OldPaddedCBOR) {
  // Test that we can decode messages padded by the old encoding function.
  for (size_t i = 0; i < 512; i++) {
    SCOPED_TRACE(i);

    const std::vector<uint8_t> dummy_array(i);
    cbor::Value::MapValue map;
    map.emplace(1, dummy_array);
    std::optional<std::vector<uint8_t>> encoded =
        EncodePaddedCBORMapOld(std::move(map));
    ASSERT_TRUE(encoded);

    std::optional<cbor::Value> decoded = DecodePaddedCBORMap(*encoded);
    ASSERT_TRUE(decoded);
  }
}

std::array<uint8_t, kP256X962Length> PublicKeyOf(const EC_KEY* private_key) {
  std::array<uint8_t, kP256X962Length> ret;
  CHECK_EQ(ret.size(),
           EC_POINT_point2oct(EC_KEY_get0_group(private_key),
                              EC_KEY_get0_public_key(private_key),
                              POINT_CONVERSION_UNCOMPRESSED, ret.data(),
                              ret.size(), /*ctx=*/nullptr));
  return ret;
}

TEST(CableV2Encoding, Digits) {
  uint8_t test_data[24];
  base::RandBytes(test_data);

  // |BytesToDigits| and |DigitsToBytes| should round-trip.
  for (size_t i = 0; i < sizeof(test_data); i++) {
    std::string digits =
        qr::BytesToDigits(base::span<const uint8_t>(test_data, i));
    std::optional<std::vector<uint8_t>> test_data_again =
        qr::DigitsToBytes(digits);
    ASSERT_TRUE(test_data_again.has_value());
    ASSERT_EQ(test_data_again.value(),
              std::vector<uint8_t>(test_data, test_data + i));
  }

  // |DigitsToBytes| should reject non-digit inputs.
  EXPECT_FALSE(qr::DigitsToBytes("a"));
  EXPECT_FALSE(qr::DigitsToBytes("ab"));
  EXPECT_FALSE(qr::DigitsToBytes("abc"));

  // |DigitsToBytes| should reject digits that can't be correct. Here three
  // digits translates into one byte, but 999 > 0xff.
  EXPECT_FALSE(qr::DigitsToBytes("999"));

  // |DigitsToBytes| should reject impossible input lengths.
  char digits[20];
  memset(digits, '0', sizeof(digits));
  for (size_t i = 0; i < sizeof(digits); i++) {
    std::optional<std::vector<uint8_t>> bytes =
        qr::DigitsToBytes(std::string_view(digits, i));
    if (!bytes.has_value()) {
      continue;
    }
    EXPECT_TRUE(base::ranges::all_of(*bytes, [](uint8_t v) { return v == 0; }));
  }

  // The encoding is used as part of an external protocol and so should not
  // change.
  static const uint8_t kTestBytes[3] = {'a', 'b', 255};
  EXPECT_EQ(qr::BytesToDigits(kTestBytes), "16736865");
}

TEST(CableV2Encoding, HandshakeSignatures) {
  static const uint8_t kSeed0[kQRSeedSize] = {0};
  static const uint8_t kSeed1[kQRSeedSize] = {1};

  bssl::UniquePtr<EC_GROUP> group(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> authenticator_key(
      EC_KEY_derive_from_secret(group.get(), kSeed0, sizeof(kSeed0)));
  bssl::UniquePtr<EC_KEY> client_key(
      EC_KEY_derive_from_secret(group.get(), kSeed1, sizeof(kSeed1)));

  const std::array<uint8_t, kP256X962Length> authenticator_public_key =
      PublicKeyOf(authenticator_key.get());
  const std::array<uint8_t, kP256X962Length> client_public_key =
      PublicKeyOf(client_key.get());

  HandshakeHash handshake_hash = {1};

  std::vector<uint8_t> signature = CalculatePairingSignature(
      authenticator_key.get(), client_public_key, handshake_hash);
  EXPECT_TRUE(VerifyPairingSignature(kSeed1, authenticator_public_key,
                                     handshake_hash, signature));

  handshake_hash[0] ^= 1;
  EXPECT_FALSE(VerifyPairingSignature(kSeed1, authenticator_public_key,
                                      handshake_hash, signature));
  handshake_hash[0] ^= 1;

  signature[0] ^= 1;
  EXPECT_FALSE(VerifyPairingSignature(kSeed1, authenticator_public_key,
                                      handshake_hash, signature));
  signature[0] ^= 1;
}

TEST(CableV2Encoding, ParseInvalidLinkingInformation) {
  const std::array<uint8_t, kQRSeedSize> local_identity_seed = {1, 2, 3, 4};
  const tunnelserver::KnownDomainID domain(0);
  const std::array<uint8_t, 32> handshake_hash = {5, 6, 7, 8};
  std::array<uint8_t, kP256X962Length> public_key = {9, 10, 11, 12};

  EXPECT_FALSE(Pairing::Parse(cbor::Value(1), domain, local_identity_seed,
                              handshake_hash));
  EXPECT_FALSE(Pairing::Parse(cbor::Value("foo"), domain, local_identity_seed,
                              handshake_hash));

  {
    cbor::Value::MapValue map;
    map.emplace(1, handshake_hash);
    EXPECT_FALSE(Pairing::Parse(cbor::Value(std::move(map)), domain,
                                local_identity_seed, handshake_hash));
  }

  {
    cbor::Value::MapValue map;
    map.emplace(1, handshake_hash);
    map.emplace(2, handshake_hash);
    map.emplace(3, handshake_hash);
    map.emplace(4, handshake_hash);
    map.emplace(5, handshake_hash);
    map.emplace(6, handshake_hash);
    EXPECT_FALSE(Pairing::Parse(cbor::Value(std::move(map)), domain,
                                local_identity_seed, handshake_hash));
  }

  {
    cbor::Value::MapValue map;
    map.emplace(1, handshake_hash);
    map.emplace(2, handshake_hash);
    map.emplace(3, handshake_hash);
    map.emplace(4, public_key);
    map.emplace(5, "test");
    map.emplace(6, handshake_hash);
    // This is structurally valid, but the public key is invalid.
    EXPECT_FALSE(Pairing::Parse(cbor::Value(std::move(map)), domain,
                                local_identity_seed, handshake_hash));
  }

  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_generate_key(key.get()));
  CHECK_EQ(sizeof(public_key),
           EC_POINT_point2oct(EC_KEY_get0_group(key.get()),
                              EC_KEY_get0_public_key(key.get()),
                              POINT_CONVERSION_UNCOMPRESSED, public_key.data(),
                              sizeof(public_key), /*ctx=*/nullptr));

  {
    cbor::Value::MapValue map;
    map.emplace(1, handshake_hash);
    map.emplace(2, handshake_hash);
    map.emplace(3, handshake_hash);
    map.emplace(4, public_key);
    map.emplace(5, "test");
    map.emplace(6, handshake_hash);
    // This is completely valid except that the signature is wrong.
    EXPECT_FALSE(Pairing::Parse(cbor::Value(std::move(map)), domain,
                                local_identity_seed, handshake_hash));
  }

  {
    bssl::UniquePtr<EC_KEY> identity_key(EC_KEY_derive_from_secret(
        EC_KEY_get0_group(key.get()), local_identity_seed.data(),
        local_identity_seed.size()));

    cbor::Value::MapValue map;
    map.emplace(1, handshake_hash);
    map.emplace(2, handshake_hash);
    map.emplace(3, handshake_hash);
    map.emplace(4, public_key);
    map.emplace(5, "test");
    map.emplace(6, CalculatePairingSignature(identity_key.get(), public_key,
                                             handshake_hash));
    // This is fully valid.
    EXPECT_TRUE(Pairing::Parse(cbor::Value(std::move(map)), domain,
                               local_identity_seed, handshake_hash));
  }
}

class CableV2HandshakeTest : public ::testing::Test {
 public:
  CableV2HandshakeTest() {
    std::fill(psk_.begin(), psk_.end(), 0);
    std::fill(identity_seed_.begin(), identity_seed_.end(), 2);

    bssl::UniquePtr<EC_GROUP> group(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    identity_key_.reset(EC_KEY_derive_from_secret(
        group.get(), identity_seed_.data(), identity_seed_.size()));
    CHECK_EQ(identity_public_.size(),
             EC_POINT_point2oct(
                 group.get(), EC_KEY_get0_public_key(identity_key_.get()),
                 POINT_CONVERSION_UNCOMPRESSED, identity_public_.data(),
                 identity_public_.size(), /*ctx=*/nullptr));
  }

 protected:
  std::array<uint8_t, 32> psk_;
  bssl::UniquePtr<EC_KEY> identity_key_;
  std::array<uint8_t, kP256X962Length> identity_public_;
  std::array<uint8_t, kQRSeedSize> identity_seed_;
};

TEST_F(CableV2HandshakeTest, MessageEncrytion) {
  std::array<uint8_t, 32> key1, key2;
  std::fill(key1.begin(), key1.end(), 1);
  std::fill(key2.begin(), key2.end(), 2);

  Crypter a(key1, key2);
  Crypter b(key2, key1);

  static constexpr size_t kMaxSize = 530;
  std::vector<uint8_t> message, ciphertext, plaintext;
  message.reserve(kMaxSize);
  ciphertext.reserve(kMaxSize);
  plaintext.reserve(kMaxSize);

  for (size_t i = 0; i < kMaxSize; i++) {
    ciphertext = message;
    ASSERT_TRUE(a.Encrypt(&ciphertext));
    ASSERT_TRUE(b.Decrypt(ciphertext, &plaintext));
    ASSERT_TRUE(plaintext == message);

    ciphertext[(13 * i) % ciphertext.size()] ^= 1;
    ASSERT_FALSE(b.Decrypt(ciphertext, &plaintext));

    message.push_back(i & 0xff);
  }
}

TEST_F(CableV2HandshakeTest, NKHandshake) {
  std::array<uint8_t, 32> wrong_psk = psk_;
  wrong_psk[0] ^= 1;

  for (const bool use_correct_key : {false, true}) {
    HandshakeInitiator initiator(use_correct_key ? psk_ : wrong_psk,
                                 identity_public_,
                                 /*identity_seed=*/std::nullopt);
    std::vector<uint8_t> message = initiator.BuildInitialMessage();
    std::vector<uint8_t> response;
    EC_KEY_up_ref(identity_key_.get());
    HandshakeResult responder_result(
        RespondToHandshake(psk_, bssl::UniquePtr<EC_KEY>(identity_key_.get()),
                           /*peer_identity=*/std::nullopt, message, &response));
    ASSERT_EQ(responder_result.has_value(), use_correct_key);
    if (!use_correct_key) {
      continue;
    }

    std::optional<std::pair<std::unique_ptr<Crypter>, HandshakeHash>>
        initiator_result(initiator.ProcessResponse(response));
    ASSERT_TRUE(initiator_result.has_value());
    EXPECT_EQ(initiator_result->second, responder_result->second);
    EXPECT_TRUE(responder_result->first->IsCounterpartyOfForTesting(
        *initiator_result->first));
    EXPECT_EQ(initiator_result->second, responder_result->second);
  }
}

TEST_F(CableV2HandshakeTest, KNHandshake) {
  std::array<uint8_t, kQRSeedSize> wrong_seed;
  crypto::RandBytes(wrong_seed);

  for (const bool use_correct_key : {false, true}) {
    SCOPED_TRACE(use_correct_key);

    base::span<const uint8_t, kQRSeedSize> seed =
        use_correct_key ? identity_seed_ : wrong_seed;
    HandshakeInitiator initiator(psk_,
                                 /*peer_identity=*/std::nullopt, seed);
    std::vector<uint8_t> message = initiator.BuildInitialMessage();
    std::vector<uint8_t> response;
    HandshakeResult responder_result(RespondToHandshake(
        psk_,
        /*identity=*/nullptr, identity_public_, message, &response));
    ASSERT_EQ(responder_result.has_value(), use_correct_key);

    if (!use_correct_key) {
      continue;
    }

    std::optional<std::pair<std::unique_ptr<Crypter>, HandshakeHash>>
        initiator_result(initiator.ProcessResponse(response));
    ASSERT_TRUE(initiator_result.has_value());
    EXPECT_TRUE(responder_result->first->IsCounterpartyOfForTesting(
        *initiator_result->first));
    EXPECT_EQ(initiator_result->second, responder_result->second);
  }
}

}  // namespace
}  // namespace cablev2
}  // namespace device
