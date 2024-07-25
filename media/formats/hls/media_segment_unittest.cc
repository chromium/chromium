// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_segment.h"

#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::hls {

TEST(HlsMediaSegmentTest, EncryptionDataIVExportTest) {
  auto iv = MediaSegment::EncryptionData::IVType::Parse(
      ResolvedSourceString::CreateForTesting(
          "0x73757065727365637265746D65737367"));
  ASSERT_TRUE(iv.has_value());
  auto data = base::MakeRefCounted<MediaSegment::EncryptionData>(
      GURL("https://example.com"), XKeyTagMethod::kAES128,
      XKeyTagKeyFormat::kIdentity, std::move(iv).value());
  ASSERT_EQ("supersecretmessg", data->GetIVStr(0).value_or(""));
}

TEST(HlsMediaSegmentTest, EncryptionDataIVExportFromSeqNum) {
  auto data = base::MakeRefCounted<MediaSegment::EncryptionData>(
      GURL("https://example.com"), XKeyTagMethod::kAES128,
      XKeyTagKeyFormat::kIdentity, std::nullopt);
  ASSERT_EQ(63, data->GetIVStr(63).value_or("")[15]);
}

}  // namespace media::hls
