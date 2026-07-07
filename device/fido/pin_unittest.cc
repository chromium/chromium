// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/pin.h"

#include "components/cbor/reader.h"
#include "crypto/keypair.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/pin_internal.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/aes.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/nid.h"

namespace device {
namespace {

using testing::ElementsAreArray;
using testing::Not;

class PINProtocolTest : public ::testing::TestWithParam<PINUVAuthProtocol> {
 protected:
  const pin::Protocol& pin_protocol() {
    return pin::ProtocolVersion(GetParam());
  }

  pin::KeyAgreementResponse PeerKeyAgreement() {
    const std::optional<pin::KeyAgreementResponse> peer_response =
        pin::KeyAgreementResponse::ParseFromCOSE(
            pin::EncodeCOSEPublicKey(base::span<const uint8_t, kP256X962Length>(
                peer_key_.ToUncompressedX962Point())));
    CHECK(peer_response);
    return *peer_response;
  }

  crypto::keypair::PrivateKey peer_key_{
      crypto::keypair::PrivateKey::GenerateEcP256()};
};

TEST_P(PINProtocolTest, EncapsulateDecapsulate) {
  // Encapsulate() and CalculateSharedKey() should yield the same shared secret.
  std::vector<uint8_t> shared_key;
  const std::array<uint8_t, kP256X962Length> platform_x962 =
      pin_protocol().Encapsulate(PeerKeyAgreement(), &shared_key);

  std::optional<crypto::keypair::PublicKey> pubkey =
      crypto::keypair::PublicKey::FromEcP256Point(platform_x962);

  EXPECT_EQ(shared_key.size(),
            GetParam() == PINUVAuthProtocol::kV1 ? 32u : 64u);
  EXPECT_THAT(pin_protocol().CalculateSharedKey(peer_key_, *pubkey),
              ElementsAreArray(shared_key));
}

TEST_P(PINProtocolTest, EncryptDecrypt) {
  constexpr char kTestPlaintext[] = "pinprotocoltestpinprotocoltest_";
  static_assert(sizeof(kTestPlaintext) % AES_BLOCK_SIZE == 0u, "");
  std::vector<uint8_t> shared_key;
  pin_protocol().Encapsulate(PeerKeyAgreement(), &shared_key);

  const std::vector<uint8_t> ciphertext =
      pin_protocol().Encrypt(shared_key, base::as_byte_span(kTestPlaintext));
  ASSERT_FALSE(ciphertext.empty());

  EXPECT_THAT(
      pin_protocol().Decrypt(shared_key, ciphertext),
      ElementsAreArray(base::span_with_nul_from_cstring(kTestPlaintext)));
}

TEST_P(PINProtocolTest, AuthenticateVerify) {
  constexpr char kTestMessage[] = "pin protocol test";
  std::vector<uint8_t> shared_key;
  pin_protocol().Encapsulate(PeerKeyAgreement(), &shared_key);

  const std::vector<uint8_t> mac =
      pin_protocol().Authenticate(shared_key, base::as_byte_span(kTestMessage));
  ASSERT_FALSE(mac.empty());

  EXPECT_TRUE(
      pin_protocol().Verify(shared_key, base::as_byte_span(kTestMessage), mac));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PINProtocolTest,
                         testing::Values(PINUVAuthProtocol::kV1,
                                         PINUVAuthProtocol::kV2));

}  // namespace
}  // namespace device
