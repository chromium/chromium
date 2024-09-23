// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/files/memory_mapped_file.h"
#include "media/base/test_data_util.h"
#include "media/parsers/ivf_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(IvfParserTest, StreamFileParsing) {
  base::FilePath file_path = GetTestDataFilePath("test-25fps.vp8");

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path)) << "Couldn't open stream file: "
                                            << file_path.MaybeAsASCII();

  IvfParser parser;
  IvfFileHeader file_header = {};

  EXPECT_TRUE(parser.Initialize(stream.data(), stream.length(), &file_header));

  // Check file header fields.
  EXPECT_EQ(base::as_byte_span(file_header.signature), kIvfHeaderSignature);
  EXPECT_EQ(0, file_header.version);
  EXPECT_EQ(sizeof(IvfFileHeader), file_header.header_size);
  EXPECT_EQ(0x30385056u, file_header.fourcc);  // VP80
  EXPECT_EQ(320u, file_header.width);
  EXPECT_EQ(240u, file_header.height);
  EXPECT_EQ(50u, file_header.timebase_denum);
  EXPECT_EQ(2u, file_header.timebase_num);
  EXPECT_EQ(250u, file_header.num_frames);

  IvfFrameHeader frame_header;
  size_t num_parsed_frames = 0;
  const uint8_t* payload = nullptr;
  while (parser.ParseNextFrame(&frame_header, &payload)) {
    ++num_parsed_frames;
    EXPECT_TRUE(payload != nullptr);

    // Only check the first frame.
    if (num_parsed_frames == 1u) {
      EXPECT_EQ(14788u, frame_header.frame_size);
      // Copy `frame_header.timestamp` into a temporary value. `EXPECT_EQ`
      // internally reads the value through a pointer, which is misaligned
      // because `IvfFrameHeader` is packed. Copy into a temporary value first.
      EXPECT_EQ(0u, uint64_t{frame_header.timestamp});
      EXPECT_EQ(
          static_cast<ptrdiff_t>(sizeof(file_header) + sizeof(frame_header)),
          payload - stream.data());
    }
  }

  EXPECT_EQ(file_header.num_frames, num_parsed_frames);
}

}  // namespace media
