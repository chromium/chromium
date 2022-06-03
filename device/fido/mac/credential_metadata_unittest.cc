// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/credential_metadata.h"

#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace device {
namespace fido {
namespace mac {
namespace {

bool MetadataEq(const CredentialMetadata& lhs, const CredentialMetadata& rhs) {
  return lhs.user_id == rhs.user_id && lhs.user_name == rhs.user_name &&
         lhs.user_display_name == rhs.user_display_name &&
         lhs.is_resident == rhs.is_resident;
}

base::span<const uint8_t> to_bytes(base::StringPiece in) {
  return base::make_span(reinterpret_cast<const uint8_t*>(in.data()),
                         in.size());
}

class CredentialMetadataTest : public ::testing::Test {
 protected:
  CredentialMetadata DefaultUser() {
    return CredentialMetadata(default_id_, "user", "user@acme.com",
                              /*is_resident=*/false);
  }

  std::vector<uint8_t> SealCredentialId(CredentialMetadata user) {
    return device::fido::mac::SealCredentialId(key_, rp_id_, std::move(user));
  }

  CredentialMetadata UnsealCredentialId(
      base::span<const uint8_t> credential_id) {
    return *device::fido::mac::UnsealCredentialId(key_, rp_id_, credential_id);
  }

  std::string EncodeRpIdAndUserId(base::StringPiece user_id) {
    return device::fido::mac::EncodeRpIdAndUserId(key_, rp_id_,
                                                  to_bytes(user_id));
  }
  std::string EncodeRpId() {
    return device::fido::mac::EncodeRpId(key_, rp_id_);
  }

  std::string DecodeRpId(const std::string& ct) {
    return *device::fido::mac::DecodeRpId(key_, ct);
  }

  std::vector<uint8_t> default_id_ = {0, 1, 2, 3};
  std::string rp_id_ = "acme.com";
  std::string key_ = "supersecretsupersecretsupersecre";
  std::string wrong_key_ = "SUPERsecretsupersecretsupersecre";
};

TEST_F(CredentialMetadataTest, CredentialId) {
  std::vector<uint8_t> id = SealCredentialId(DefaultUser());
  EXPECT_EQ(1, (id)[0]);
  EXPECT_EQ(55u, id.size());
  EXPECT_TRUE(MetadataEq(UnsealCredentialId(id), DefaultUser()));
}

TEST_F(CredentialMetadataTest, LegacyCredentialId) {
  auto user = DefaultUser();
  std::vector<uint8_t> id = SealLegacyV0CredentialIdForTestingOnly(
      key_, rp_id_, user.user_id, user.user_name, user.user_display_name);
  EXPECT_EQ(0, id[0]);
  EXPECT_EQ(54u, id.size());
  EXPECT_TRUE(MetadataEq(UnsealCredentialId(id), DefaultUser()));
}

TEST_F(CredentialMetadataTest, CredentialId_IsRandom) {
  EXPECT_NE(SealCredentialId(DefaultUser()), SealCredentialId(DefaultUser()));
}

TEST_F(CredentialMetadataTest, CredentialId_FailDecode) {
  const auto id = SealCredentialId(DefaultUser());
  // Flipping a bit in version, nonce, or ciphertext will fail credential
  // decryption.
  for (size_t i = 0; i < id.size(); i++) {
    std::vector<uint8_t> new_id(id);
    new_id[i] = new_id[i] ^ 0x01;
    EXPECT_FALSE(device::fido::mac::UnsealCredentialId(key_, rp_id_, new_id));
  }
}

TEST_F(CredentialMetadataTest, CredentialId_InvalidRp) {
  std::vector<uint8_t> id = SealCredentialId(DefaultUser());
  // The credential id is authenticated with the RP, thus decryption under a
  // different RP fails.
  EXPECT_FALSE(device::fido::mac::UnsealCredentialId(key_, "notacme.com", id));
}

TEST_F(CredentialMetadataTest, EncodeRpIdAndUserId) {
  EXPECT_EQ(64u, EncodeRpIdAndUserId("user@acme.com").size())
      << EncodeRpIdAndUserId("user@acme.com");

  EXPECT_EQ(EncodeRpIdAndUserId("user"), EncodeRpIdAndUserId("user"));
  EXPECT_NE(EncodeRpIdAndUserId("userA"), EncodeRpIdAndUserId("userB"));
  EXPECT_NE(EncodeRpIdAndUserId("user"),
            device::fido::mac::EncodeRpIdAndUserId(key_, "notacme.com",
                                                   to_bytes("user")));
  EXPECT_NE(EncodeRpIdAndUserId("user"),
            device::fido::mac::EncodeRpIdAndUserId(wrong_key_, rp_id_,
                                                   to_bytes("user")));
}

TEST_F(CredentialMetadataTest, EncodeRpId) {
  EXPECT_EQ(48u, EncodeRpId().size());

  EXPECT_EQ(EncodeRpId(), EncodeRpId());
  EXPECT_NE(EncodeRpId(), device::fido::mac::EncodeRpId(key_, "notacme.com"));
  EXPECT_NE(EncodeRpId(), device::fido::mac::EncodeRpId(wrong_key_, rp_id_));
}

TEST_F(CredentialMetadataTest, DecodeRpId) {
  EXPECT_EQ(rp_id_, DecodeRpId(EncodeRpId()));
  EXPECT_NE(rp_id_,
            *device::fido::mac::DecodeRpId(
                key_, device::fido::mac::EncodeRpId(key_, "notacme.com")));
  EXPECT_FALSE(device::fido::mac::DecodeRpId(wrong_key_, EncodeRpId()));
}

TEST_F(CredentialMetadataTest, Truncation) {
  constexpr char len70[] =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef012345";
  constexpr char len71[] =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456";
  constexpr char truncated[] =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef012…";
  auto credential_id =
      SealCredentialId(CredentialMetadata({1}, len71, len71, false));
  CredentialMetadata metadata = UnsealCredentialId(credential_id);
  EXPECT_EQ(metadata.user_name, truncated);
  EXPECT_EQ(metadata.user_display_name, truncated);

  credential_id =
      SealCredentialId(CredentialMetadata({1}, len70, len70, false));
  metadata = UnsealCredentialId(credential_id);
  EXPECT_EQ(metadata.user_name, len70);
  EXPECT_EQ(metadata.user_display_name, len70);
}

TEST(CredentialMetadata, GenerateCredentialMetadataSecret) {
  std::string s1 = GenerateCredentialMetadataSecret();
  EXPECT_EQ(32u, s1.size());
  std::string s2 = GenerateCredentialMetadataSecret();
  EXPECT_EQ(32u, s2.size());
  EXPECT_NE(s1, s2);
}

TEST(CredentialMetadata, FromPublicKeyCredentialUserEntity) {
  std::vector<uint8_t> user_id = {{1, 2, 3}};
  PublicKeyCredentialUserEntity in(user_id);
  in.name = "username";
  in.display_name = "display name";
  in.icon_url = GURL("http://rp.foo/user.png");
  CredentialMetadata out =
      CredentialMetadata::FromPublicKeyCredentialUserEntity(
          std::move(in), /*is_resident=*/false);
  EXPECT_EQ(user_id, out.user_id);
  EXPECT_EQ("username", out.user_name);
  EXPECT_EQ("display name", out.user_display_name);
  EXPECT_FALSE(out.is_resident);
}

TEST(CredentialMetadata, ToPublicKeyCredentialUserEntity) {
  std::vector<uint8_t> user_id = {{1, 2, 3}};
  CredentialMetadata in(user_id, "username", "display name",
                        /*is_resident=*/false);
  PublicKeyCredentialUserEntity out = in.ToPublicKeyCredentialUserEntity();
  EXPECT_EQ(user_id, out.id);
  EXPECT_EQ("username", out.name.value());
  EXPECT_EQ("display name", out.display_name.value());
  EXPECT_FALSE(out.icon_url.has_value());
}

}  // namespace
}  // namespace mac
}  // namespace fido
}  // namespace device
