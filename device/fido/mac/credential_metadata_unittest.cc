// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/credential_metadata.h"

#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::fido::mac {
namespace {

bool MetadataEq(const CredentialMetadata& lhs, const CredentialMetadata& rhs) {
  return lhs.version == rhs.version && lhs.user_id == rhs.user_id &&
         lhs.user_name == rhs.user_name &&
         lhs.user_display_name == rhs.user_display_name &&
         lhs.is_resident == rhs.is_resident;
}

class CredentialMetadataTest : public ::testing::Test {
 protected:
  CredentialMetadata DefaultUser() {
    return CredentialMetadata(CredentialMetadata::CurrentVersion(), default_id_,
                              "user", "user@acme.com",
                              /*is_resident=*/false,
                              CredentialMetadata::SignCounter::kZero);
  }

  std::vector<uint8_t> SealCredentialMetadata(CredentialMetadata user) {
    return device::fido::mac::SealCredentialMetadata(key_, rp_id_,
                                                     std::move(user));
  }

  CredentialMetadata UnsealCredentialMetadata(base::span<const uint8_t> in) {
    return *device::fido::mac::UnsealMetadataFromApplicationTag(key_, rp_id_,
                                                                in);
  }

  CredentialMetadata UnsealLegacyCredentialId(
      base::span<const uint8_t> credential_id) {
    return *device::fido::mac::UnsealMetadataFromLegacyCredentialId(
        key_, rp_id_, credential_id);
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

TEST_F(CredentialMetadataTest, CredentialMetadata) {
  for (auto version :
       {CredentialMetadata::Version::kV3, CredentialMetadata::Version::kV4}) {
    auto metadata = DefaultUser();
    metadata.version = version;
    std::vector<uint8_t> sealed = SealCredentialMetadata(metadata);
    EXPECT_EQ(sealed.size(), 55u);
    EXPECT_TRUE(MetadataEq(UnsealCredentialMetadata(sealed), metadata));
  }
}

TEST_F(CredentialMetadataTest, LegacyCredentialIds) {
  for (auto version :
       {CredentialMetadata::Version::kV0, CredentialMetadata::Version::kV1,
        CredentialMetadata::Version::kV2}) {
    SCOPED_TRACE(testing::Message() << "version=" << int(version));
    auto user = DefaultUser();
    bool is_resident = version > CredentialMetadata::Version::kV0;
    std::vector<uint8_t> id = SealLegacyCredentialIdForTestingOnly(
        version, key_, rp_id_, user.user_id, user.user_name,
        user.user_display_name, is_resident);
    switch (version) {
      case CredentialMetadata::Version::kV0:
        EXPECT_EQ(id[0], static_cast<uint8_t>(version));
        EXPECT_EQ(id.size(), 54u);
        break;
      case CredentialMetadata::Version::kV1:
        EXPECT_EQ(id[0], static_cast<uint8_t>(version));
        EXPECT_EQ(id.size(), 55u);
        break;
      case CredentialMetadata::Version::kV2:
        EXPECT_EQ(id.size(), 54u);
        break;
      default:
        NOTREACHED();
    }
    CredentialMetadata expected = DefaultUser();
    expected.version = version;
    expected.is_resident = is_resident;
    expected.sign_counter_type =
        version < CredentialMetadata::Version::kV2
            ? CredentialMetadata::SignCounter::kTimestamp
            : CredentialMetadata::SignCounter::kZero;
    EXPECT_TRUE(MetadataEq(UnsealLegacyCredentialId(id), expected));
  }
}

TEST_F(CredentialMetadataTest, CredentialMetadata_FailDecode) {
  const auto id = SealCredentialMetadata(DefaultUser());
  // Flipping a bit in nonce, or ciphertext will fail metadata decryption.
  for (size_t i = 0; i < id.size(); i++) {
    std::vector<uint8_t> new_id(id);
    new_id[i] = new_id[i] ^ 0x01;
    EXPECT_FALSE(device::fido::mac::UnsealMetadataFromApplicationTag(
        key_, rp_id_, new_id));
  }
}

TEST_F(CredentialMetadataTest, CredentialMetadata_InvalidRp) {
  std::vector<uint8_t> id = SealCredentialMetadata(DefaultUser());
  // The credential id is authenticated with the RP, thus decryption under a
  // different RP fails.
  EXPECT_FALSE(device::fido::mac::UnsealMetadataFromApplicationTag(
      key_, "notacme.com", id));
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
  auto credential_id = SealCredentialMetadata(CredentialMetadata(
      CredentialMetadata::CurrentVersion(), {1}, len71, len71,
      /*is_resident=*/false, CredentialMetadata::SignCounter::kZero));
  CredentialMetadata metadata = UnsealCredentialMetadata(credential_id);
  EXPECT_EQ(metadata.user_name, truncated);
  EXPECT_EQ(metadata.user_display_name, truncated);

  credential_id = SealCredentialMetadata(CredentialMetadata(
      CredentialMetadata::CurrentVersion(), {1}, len70, len70,
      /*is_resident=*/false, CredentialMetadata::SignCounter::kZero));
  metadata = UnsealCredentialMetadata(credential_id);
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
  CredentialMetadata out =
      CredentialMetadata::FromPublicKeyCredentialUserEntity(
          std::move(in), /*is_resident=*/false);
  EXPECT_EQ(user_id, out.user_id);
  EXPECT_EQ("username", out.user_name);
  EXPECT_EQ("display name", out.user_display_name);
  EXPECT_FALSE(out.is_resident);
  EXPECT_EQ(out.sign_counter_type, CredentialMetadata::SignCounter::kZero);
}

TEST(CredentialMetadata, ToPublicKeyCredentialUserEntity) {
  std::vector<uint8_t> user_id = {{1, 2, 3}};
  CredentialMetadata in(
      CredentialMetadata::CurrentVersion(), user_id, "username", "display name",
      /*is_resident=*/false, CredentialMetadata::SignCounter::kZero);
  PublicKeyCredentialUserEntity out = in.ToPublicKeyCredentialUserEntity();
  EXPECT_EQ(user_id, out.id);
  EXPECT_EQ("username", out.name.value());
  EXPECT_EQ("display name", out.display_name.value());
}

}  // namespace
}  // namespace device::fido::mac
