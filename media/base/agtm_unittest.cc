// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/agtm.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkData.h"
#include "ui/gfx/switches.h"

namespace media {

class AgtmTest : public testing::Test {
 public:
  AgtmTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kHdrAgtm, features::kHdrAgtmParseOldSyntax}, {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AgtmTest, HasMetadata) {
  const std::vector<uint8_t> data = {0xB5, 0x00, 0x90, 0x00, 0x01,
                                     0x00, 0x80, 0x07, 0xd0};
  gfx::HDRMetadata hdr_metadata;
  SetAgtmFromT35(hdr_metadata, data);
  EXPECT_TRUE(hdr_metadata.HasAgtm());
}

TEST_F(AgtmTest, HasMetadataWithCountryCode) {
  const uint8_t t35_country_code = 0xB5;
  const std::vector<uint8_t> data = {0x00, 0x90, 0x00, 0x01,
                                     0x00, 0x80, 0x07, 0xd0};
  gfx::HDRMetadata hdr_metadata;
  SetAgtmFromT35WithCountryCode(hdr_metadata, t35_country_code, data);
  EXPECT_TRUE(hdr_metadata.HasAgtm());
}

TEST_F(AgtmTest, WrongType) {
  const uint8_t t35_country_code = 0xB5;
  const std::vector<uint8_t> data = {0x00, 0x90, 0x00, 0xff /* wrong value*/,
                                     0x00, 0x80, 0x07, 0xd0};
  gfx::HDRMetadata hdr_metadata;
  SetAgtmFromT35WithCountryCode(hdr_metadata, t35_country_code, data);
  EXPECT_FALSE(hdr_metadata.HasAgtm());
}

TEST_F(AgtmTest, DataTooShort) {
  const uint8_t t35_country_code = 0xB5;
  const std::vector<uint8_t> data = {0x58, 0x90, 0x69};
  gfx::HDRMetadata hdr_metadata;
  SetAgtmFromT35WithCountryCode(hdr_metadata, t35_country_code, data);
  EXPECT_FALSE(hdr_metadata.HasAgtm());
}

}  // namespace media
