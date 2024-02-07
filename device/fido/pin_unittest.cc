// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/pin.h"

#include "components/cbor/reader.h"
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
  void SetUp() override {
    peer_key_.reset(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    CHECK(EC_KEY_generate_key(peer_key_.get()));
  }

  const pin::Protocol& pin_protocol() {
    return pin::ProtocolVersion(GetParam());
  }

  pin::KeyAgreementResponse PeerKeyAgreement() {
    std::array<uint8_t, kP256X962Length> peer_x962;
    CHECK_EQ(EC_POINT_point2oct(EC_KEY_get0_group(peer_key_.get()),
                                EC_KEY_get0_public_key(peer_key_.get()),
                                POINT_CONVERSION_UNCOMPRESSED, peer_x962.data(),
                                peer_x962.size(), nullptr /* BN_CTX */),
             peer_x962.size());
    const std::optional<pin::KeyAgreementResponse> peer_response =
        pin::KeyAgreementResponse::ParseFromCOSE(
            pin::EncodeCOSEPublicKey(peer_x962));
    CHECK(peer_response);
    return *peer_response;
  }

  EC_KEY* peer_key() { return peer_key_.get(); }

  bssl::UniquePtr<EC_KEY> peer_key_;
};

TEST_P(PINProtocolTest, EncapsulateDecapsulate) {
  // Encapsulate() and CalculateSharedKey() should yield the same shared secret.
  std::vector<uint8_t> shared_key;
  const std::array<uint8_t, kP256X962Length> platform_x962 =
      pin_protocol().Encapsulate(PeerKeyAgreement(), &shared_key);

  const bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  const bssl::UniquePtr<EC_POINT> platform_point(EC_POINT_new(p256.get()));
  ASSERT_TRUE(EC_POINT_oct2point(p256.get(), platform_point.get(),
                                 platform_x962.data(), platform_x962.size(),
                                 /*ctx=*/nullptr));

  EXPECT_EQ(shared_key.size(),
            GetParam() == PINUVAuthProtocol::kV1 ? 32u : 64u);
  EXPECT_THAT(
      pin_protocol().CalculateSharedKey(peer_key(), platform_point.get()),
      ElementsAreArray(shared_key));
}

TEST_P(PINProtocolTest, EncryptDecrypt) {
  constexpr char kTestPlaintext[] = "pinprotocoltestpinprotocoltest_";
  static_assert(sizeof(kTestPlaintext) % AES_BLOCK_SIZE == 0u, "");
  std::vector<uint8_t> shared_key;
  pin_protocol().Encapsulate(PeerKeyAgreement(), &shared_key);

  const std::vector<uint8_t> ciphertext = pin_protocol().Encrypt(
      shared_key, base::as_bytes(base::make_span(kTestPlaintext)));
  ASSERT_FALSE(ciphertext.empty());

  EXPECT_THAT(pin_protocol().Decrypt(shared_key, ciphertext),
              ElementsAreArray(base::make_span(kTestPlaintext)));
}

TEST_P(PINProtocolTest, AuthenticateVerify) {
  constexpr char kTestMessage[] = "pin protocol test";
  std::vector<uint8_t> shared_key;
  pin_protocol().Encapsulate(PeerKeyAgreement(), &shared_key);

  const std::vector<uint8_t> mac = pin_protocol().Authenticate(
      shared_key, base::as_bytes(base::make_span(kTestMessage)));
  ASSERT_FALSE(mac.empty());

  EXPECT_TRUE(pin_protocol().Verify(
      shared_key, base::as_bytes(base::make_span(kTestMessage)), mac));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PINProtocolTest,
                         testing::Values(PINUVAuthProtocol::kV1,
                                         PINUVAuthProtocol::kV2));

}  // namespace
}  // namespace device
