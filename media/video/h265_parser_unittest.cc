// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/h265_parser.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(H265ParserTest, RawHevcStreamFileParsing) {
  base::FilePath file_path = GetTestDataFilePath("bear.hevc");
  // Number of NALUs in the test stream to be parsed.
  const int num_nalus = 35;

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  H265Parser parser;
  parser.SetStream(stream.data(), stream.length());

  // Parse until the end of stream/unsupported stream/error in stream is found.
  int num_parsed_nalus = 0;
  while (true) {
    H265NALU nalu;
    H265Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res == H265Parser::kEOStream) {
      DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
               << num_parsed_nalus;
      ASSERT_EQ(num_nalus, num_parsed_nalus);
      return;
    }
    ASSERT_EQ(res, H265Parser::kOk);

    ++num_parsed_nalus;
    DVLOG(4) << "Found NALU " << nalu.nal_unit_type;

    switch (nalu.nal_unit_type) {
      case H265NALU::SPS_NUT:
        int sps_id;
        res = parser.ParseSPS(&sps_id);
        ASSERT_TRUE(!!parser.GetSPS(sps_id));
        break;
      default:
        break;
    }
    ASSERT_EQ(res, H265Parser::kOk);
  }
}

}  // namespace media
