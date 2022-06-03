// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>

#include "net/disk_cache/simple/simple_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using disk_cache::simple_util::ConvertEntryHashKeyToHexString;
using disk_cache::simple_util::GetEntryHashKeyAsHexString;
using disk_cache::simple_util::GetEntryHashKeyFromHexString;
using disk_cache::simple_util::GetEntryHashKey;
using disk_cache::simple_util::GetFileSizeFromDataSize;
using disk_cache::simple_util::GetDataSizeFromFileSize;

class SimpleUtilTest : public testing::Test {};

TEST_F(SimpleUtilTest, ConvertEntryHashKeyToHexString) {
  EXPECT_EQ("0000000005f5e0ff",
            ConvertEntryHashKeyToHexString(UINT64_C(99999999)));
  EXPECT_EQ("7fffffffffffffff",
            ConvertEntryHashKeyToHexString(UINT64_C(9223372036854775807)));
  EXPECT_EQ("8000000000000000",
            ConvertEntryHashKeyToHexString(UINT64_C(9223372036854775808)));
  EXPECT_EQ("ffffffffffffffff",
            ConvertEntryHashKeyToHexString(UINT64_C(18446744073709551615)));
}

TEST_F(SimpleUtilTest, GetEntryHashKey) {
  EXPECT_EQ("7ac408c1dff9c84b",
            GetEntryHashKeyAsHexString("http://www.amazon.com/"));
  EXPECT_EQ(UINT64_C(0x7ac408c1dff9c84b),
            GetEntryHashKey("http://www.amazon.com/"));

  EXPECT_EQ("9fe947998c2ccf47",
            GetEntryHashKeyAsHexString("www.amazon.com"));
  EXPECT_EQ(UINT64_C(0x9fe947998c2ccf47), GetEntryHashKey("www.amazon.com"));

  EXPECT_EQ("0d4b6b5eeea339da", GetEntryHashKeyAsHexString(""));
  EXPECT_EQ(UINT64_C(0x0d4b6b5eeea339da), GetEntryHashKey(""));

  EXPECT_EQ("a68ac2ecc87dfd04", GetEntryHashKeyAsHexString("http://www.domain.com/uoQ76Kb2QL5hzaVOSAKWeX0W9LfDLqphmRXpsfHN8tgF5lCsfTxlOVWY8vFwzhsRzoNYKhUIOTc5TnUlT0vpdQflPyk2nh7vurXOj60cDnkG3nsrXMhFCsPjhcZAic2jKpF9F9TYRYQwJo81IMi6gY01RK3ZcNl8WGfqcvoZ702UIdetvR7kiaqo1czwSJCMjRFdG6EgMzgXrwE8DYMz4fWqoa1F1c1qwTCBk3yOcmGTbxsPSJK5QRyNea9IFLrBTjfE7ZlN2vZiI7adcDYJef.htm"));

  EXPECT_EQ(UINT64_C(0xa68ac2ecc87dfd04), GetEntryHashKey("http://www.domain.com/uoQ76Kb2QL5hzaVOSAKWeX0W9LfDLqphmRXpsfHN8tgF5lCsfTxlOVWY8vFwzhsRzoNYKhUIOTc5TnUlT0vpdQflPyk2nh7vurXOj60cDnkG3nsrXMhFCsPjhcZAic2jKpF9F9TYRYQwJo81IMi6gY01RK3ZcNl8WGfqcvoZ702UIdetvR7kiaqo1czwSJCMjRFdG6EgMzgXrwE8DYMz4fWqoa1F1c1qwTCBk3yOcmGTbxsPSJK5QRyNea9IFLrBTjfE7ZlN2vZiI7adcDYJef.htm"));
}

TEST_F(SimpleUtilTest, GetEntryHashKeyFromHexString) {
  uint64_t hash_key = 0;
  EXPECT_TRUE(GetEntryHashKeyFromHexString("0000000005f5e0ff", &hash_key));
  EXPECT_EQ(UINT64_C(99999999), hash_key);

  EXPECT_TRUE(GetEntryHashKeyFromHexString("7ffffffffffffffF", &hash_key));
  EXPECT_EQ(UINT64_C(9223372036854775807), hash_key);

  EXPECT_TRUE(GetEntryHashKeyFromHexString("8000000000000000", &hash_key));
  EXPECT_EQ(UINT64_C(9223372036854775808), hash_key);

  EXPECT_TRUE(GetEntryHashKeyFromHexString("FFFFFFFFFFFFFFFF", &hash_key));
  EXPECT_EQ(UINT64_C(18446744073709551615), hash_key);

  // Wrong hash string size.
  EXPECT_FALSE(GetEntryHashKeyFromHexString("FFFFFFFFFFFFFFF", &hash_key));

  // Wrong hash string size.
  EXPECT_FALSE(GetEntryHashKeyFromHexString("FFFFFFFFFFFFFFFFF", &hash_key));

  EXPECT_FALSE(GetEntryHashKeyFromHexString("iwr8wglhg8*(&1231((", &hash_key));
}

TEST_F(SimpleUtilTest, SizesAndOffsets) {
  const std::string key("This is an example key");
  const int data_size = 1000;
  const int file_size = GetFileSizeFromDataSize(key.size(), data_size);
  EXPECT_EQ(data_size, GetDataSizeFromFileSize(key.size(), file_size));
}
