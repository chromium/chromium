// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/agtm.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkData.h"

namespace media {

TEST(GetSerializedAgtmItutT35, HasMetadata) {
  const uint8_t t35_country_code = 0xB5;
  const std::vector<uint8_t> data = {0x00, 0x90, 0x00, 0x01, 0x01,
                                     0x00, 0x01, 0x02, 0x03};
  const auto agtm = GetSerializedAgtmItutT35(t35_country_code, data);
  ASSERT_TRUE(agtm);
  EXPECT_EQ(agtm->size(), 5u);
  const std::vector<uint8_t> expected = {0x01, 0x00, 0x01, 0x02, 0x03};
  EXPECT_TRUE(SkData::Equals(
      agtm.get(),
      SkData::MakeWithCopy(expected.data(), expected.size()).get()));
}

TEST(GetSerializedAgtmItutT35, WrongType) {
  const uint8_t t35_country_code = 0xB5;
  const std::vector<uint8_t> data = {
      0x00, 0x90, 0x00, 0xff /* wrong value*/, 0x01, 0x00, 0x01, 0x02, 0x03};
  const auto agtm = GetSerializedAgtmItutT35(t35_country_code, data);
  ASSERT_FALSE(agtm);
}

TEST(GetSerializedAgtmItutT35, DataTooShort) {
  const uint8_t t35_country_code = 0xB5;
  const std::vector<uint8_t> data = {0x58, 0x90, 0x69};
  const auto agtm = GetSerializedAgtmItutT35(t35_country_code, data);
  ASSERT_FALSE(agtm);
}

}  // namespace media
