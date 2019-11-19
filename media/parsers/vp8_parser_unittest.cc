// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/filters/ivf_parser.h"
#include "media/parsers/vp8_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(Vp8ParserTest, StreamFileParsing) {
  base::FilePath file_path = GetTestDataFilePath("test-25fps.vp8");
  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  IvfParser ivf_parser;
  IvfFileHeader ivf_file_header = {};
  ASSERT_TRUE(
      ivf_parser.Initialize(stream.data(), stream.length(), &ivf_file_header));
  ASSERT_EQ(ivf_file_header.fourcc, 0x30385056u);  // VP80

  Vp8Parser vp8_parser;
  IvfFrameHeader ivf_frame_header = {};
  size_t num_parsed_frames = 0;

  // Parse until the end of stream/unsupported stream/error in stream is found.
  const uint8_t* payload = nullptr;
  while (ivf_parser.ParseNextFrame(&ivf_frame_header, &payload)) {
    Vp8FrameHeader fhdr;

    ASSERT_TRUE(
        vp8_parser.ParseFrame(payload, ivf_frame_header.frame_size, &fhdr));

    ++num_parsed_frames;
  }

  DVLOG(1) << "Number of successfully parsed frames before EOS: "
           << num_parsed_frames;

  EXPECT_EQ(ivf_file_header.num_frames, num_parsed_frames);
}

}  // namespace media
