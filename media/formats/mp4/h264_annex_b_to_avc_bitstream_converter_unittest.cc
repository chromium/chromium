// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "media/formats/mp4/box_definitions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::vector<uint8_t> ReadTestFile(std::string name) {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.Append(FILE_PATH_LITERAL(
                         "media/formats/mp4/h264_annex_b_fuzz_corpus"))
             .AppendASCII(name);
  base::File f(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  DCHECK(f.IsValid()) << "path: " << path.AsUTF8Unsafe();
  auto size = f.GetLength();

  std::vector<uint8_t> result(size);
  if (size > 0)
    f.ReadAtCurrentPosAndCheck(result);
  return result;
}

}  // namespace

namespace media {

TEST(H264AnnexBToAvcBitstreamConverterTest, Success) {
  std::string chunks[] = {"chunk1-config-idr.bin", "chunk2-non-idr.bin",
                          "chunk3-non-idr.bin",    "chunk4-non-idr.bin",
                          "chunk5-non-idr.bin",    "chunk6-config-idr.bin",
                          "chunk7-non-idr.bin"};
  H264AnnexBToAvcBitstreamConverter converter;

  for (std::string& name : chunks) {
    SCOPED_TRACE(name);
    auto input = ReadTestFile(name);
    SCOPED_TRACE(input.size());
    std::vector<uint8_t> output;
    size_t desired_size = 0;
    bool config_changed = false;

    auto status =
        converter.ConvertChunk(input, output, &config_changed, &desired_size);
    ASSERT_EQ(status.code(), StatusCode::kH264BufferTooSmall);
    output.resize(desired_size);

    status = converter.ConvertChunk(input, output, &config_changed, nullptr);
    EXPECT_TRUE(status.is_ok()) << status.message();

    auto& config = converter.GetCurrentConfig();
    if (name.find("config") != std::string::npos) {
      // Chunks with configuration
      EXPECT_TRUE(config_changed);

      EXPECT_EQ(config.version, 1);
      EXPECT_EQ(config.profile_indication, 66);
      EXPECT_EQ(config.profile_compatibility, 3);
      EXPECT_EQ(config.avc_level, 30);
      EXPECT_EQ(config.length_size, 4);
      EXPECT_EQ(config.sps_list.size(), 1ul);
      EXPECT_EQ(config.pps_list.size(), 1ul);
    } else {
      EXPECT_FALSE(config_changed);
    }

    std::vector<uint8_t> config_bin;
    EXPECT_TRUE(config.Serialize(config_bin)) << " file: " << name;
  }
}

TEST(H264AnnexBToAvcBitstreamConverterTest, Failure) {
  H264AnnexBToAvcBitstreamConverter converter;

  std::vector<uint8_t> input{0x0, 0x0, 0x0, 0x1, 0x9,  0x10,
                             0x0, 0x0, 0x0, 0x1, 0x67, 0x42};
  std::vector<uint8_t> output(input.size());

  auto status = converter.ConvertChunk(input, output, nullptr, nullptr);
  ASSERT_EQ(status.code(), StatusCode::kH264ParsingError);
}

}  // namespace media
