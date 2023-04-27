// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/region_capture_crop_id.h"
#include "base/token.h"
#include "base/uuid.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(RegionCaptureCropIdTest, GUIDToToken) {
  const base::Uuid kGUID =
      base::Uuid::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e");
  ASSERT_TRUE(kGUID.is_valid());
  EXPECT_EQ(GUIDToToken(kGUID),
            base::Token(0x21abd97f'73e84b88u, 0x9389a9fe'e6abda5eu));

  const base::Uuid kMinGUID =
      base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000000");
  ASSERT_TRUE(kMinGUID.is_valid());
  EXPECT_EQ(GUIDToToken(kMinGUID), base::Token(0, 0));

  const base::Uuid kMaxGUID =
      base::Uuid::ParseLowercase("ffffffff-ffff-ffff-ffff-ffffffffffff");
  ASSERT_TRUE(kMaxGUID.is_valid());
  EXPECT_EQ(GUIDToToken(kMaxGUID),
            base::Token(0xffffffff'ffffffffu, 0xffffffff'ffffffffu));

  // Empty strings are patently not of the expected format. Parsing them
  // yields an invalid/empty GUID. Calling AsToken() on such a base::Uuid yields
  // an empty/invalid Token.
  const base::Uuid kEmptyGUID = base::Uuid::ParseLowercase("");
  ASSERT_FALSE(kEmptyGUID.is_valid());
  EXPECT_EQ(GUIDToToken(kEmptyGUID), base::Token());
}

TEST(RegionCaptureCropIdTest, TokenToGUID) {
  const base::Token kToken(0x21abd97f'73e84b88u, 0x9389a9fe'e6abda5eu);
  EXPECT_TRUE(TokenToGUID(kToken).is_valid());
  EXPECT_EQ(TokenToGUID(kToken),
            base::Uuid::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e"));

  const base::Token kMinToken(0u, 0u);
  EXPECT_TRUE(TokenToGUID(kMinToken).is_valid());
  EXPECT_EQ(TokenToGUID(kMinToken),
            base::Uuid::ParseLowercase("00000000-0000-0000-0000-000000000000"));

  const base::Token kMaxToken(0xffffffff'ffffffffu, 0xffffffff'ffffffffu);
  EXPECT_TRUE(TokenToGUID(kMaxToken).is_valid());
  EXPECT_EQ(TokenToGUID(kMaxToken),
            base::Uuid::ParseLowercase("ffffffff-ffff-ffff-ffff-ffffffffffff"));
}

TEST(RegionCaptureCropIdTest, RandomRoundTripConversion) {
  // Token -> GUID -> Token
  const base::Token token = base::Token::CreateRandom();
  EXPECT_EQ(token, GUIDToToken(TokenToGUID(token)));

  // GUID -> Token -> GUID
  const base::Uuid guid = base::Uuid::GenerateRandomV4();
  EXPECT_EQ(guid, TokenToGUID(GUIDToToken(guid)));
}

}  // namespace
}  // namespace blink
