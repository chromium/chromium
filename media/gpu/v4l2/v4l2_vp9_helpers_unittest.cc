// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/gpu/v4l2/v4l2_vp9_helpers.h"

#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/files/memory_mapped_file.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/parsers/ivf_parser.h"
#include "media/parsers/vp9_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {
// Append |frame_sizes| to |decoder_buffer|'s side_data.
void AppendSideData(DecoderBuffer& decoder_buffer,
                    const std::vector<uint32_t>& frame_sizes) {
  decoder_buffer.WritableSideData().spatial_layers = frame_sizes;
}
}  // namespace

// Checks superframe index size is expected.
TEST(V4L2VP9HelpersTest, CheckSuperFrameIndexSize) {
  constexpr uint32_t kFrameSizes[] = {
      0x10,       // 1 byte
      0x1020,     // 2 byte
      0x010203,   // 3 byte
      0x01020304  // 4 byte
  };

  constexpr size_t kNumFrames = std::size(kFrameSizes);
  for (size_t mask = 1; mask < (1 << kNumFrames) - 1; mask++) {
    size_t buffer_size = 0;
    size_t expected_bytes_per_framesize = 0;
    std::vector<uint32_t> frame_sizes;
    for (size_t i = 0; i < kNumFrames; i++) {
      if (!(mask & (1 << i))) {
        continue;
      }
      frame_sizes.push_back(UNSAFE_TODO(kFrameSizes[i]));
      buffer_size += UNSAFE_TODO(kFrameSizes[i]);
      expected_bytes_per_framesize = i + 1;
    }

    // Since we don't care the buffer content, the buffer is zero except VP9
    // frame marker.
    std::vector<uint8_t> tmp_buffer(buffer_size);
    size_t offset = 0;
    for (const uint32_t frame_size : frame_sizes) {
      uint8_t* header = UNSAFE_TODO(tmp_buffer.data() + offset);
      *header = 0x8f;
      offset += frame_size;
    }
    auto decoder_buffer = DecoderBuffer::CopyFrom(tmp_buffer);
    AppendSideData(*decoder_buffer, frame_sizes);

    AppendVP9SuperFrameIndex(decoder_buffer);
    if (frame_sizes.size() == 1) {
      EXPECT_EQ(decoder_buffer->size(), buffer_size);
      continue;
    }

    EXPECT_GT(decoder_buffer->size(), buffer_size);
    size_t superframe_index_size = decoder_buffer->size() - buffer_size;
    EXPECT_EQ(superframe_index_size,
              2 + expected_bytes_per_framesize * frame_sizes.size());
  }
}

TEST(V4L2VP9HelpersTest, ParseAppendedSuperFrameIndex) {
  auto stream = std::make_unique<base::MemoryMappedFile>();
  ASSERT_TRUE(stream->Initialize(GetTestDataFilePath("test-25fps.vp9")));

  // Read three frames from test-25fps.vp9.
  IvfParser ivf_parser;
  IvfFileHeader ivf_file_header;
  ASSERT_TRUE(ivf_parser.Initialize(stream->bytes(), &ivf_file_header));
  ASSERT_EQ(ivf_file_header.fourcc, 0x30395056u);  // VP90

  constexpr size_t kNumBuffers = 3;
  std::array<base::span<const uint8_t>, kNumBuffers> buffers;
  for (auto& span_buffer : buffers) {
    IvfFrameHeader ivf_frame_header;
    span_buffer = ivf_parser.ParseNextFrame(&ivf_frame_header);
    ASSERT_FALSE(span_buffer.empty());
  }

  std::vector<uint32_t> frame_sizes;
  std::vector<uint8_t> merged_buffer;
  for (size_t i = 0; i < kNumBuffers; ++i) {
    frame_sizes.push_back(buffers[i].size());

    // |merged_buffer| is composed of [0, i] frames.
    const size_t offset = merged_buffer.size();
    merged_buffer.resize(offset + buffers[i].size());
    UNSAFE_TODO(memcpy(merged_buffer.data() + offset, buffers[i].data(),
                       buffers[i].size()));

    auto decoder_buffer = DecoderBuffer::CopyFrom(merged_buffer);
    AppendSideData(*decoder_buffer, frame_sizes);

    AppendVP9SuperFrameIndex(decoder_buffer);
    Vp9Parser vp9_parser;
    vp9_parser.SetStream(*decoder_buffer,
                         /*stream_config=*/nullptr);

    // Parse the merged buffer with the created superframe index.
    for (size_t j = 0; j <= i; j++) {
      Vp9FrameHeader frame_header;
      gfx::Size allocate_size;
      std::unique_ptr<DecryptConfig> frame_decrypt_config;
      EXPECT_EQ(vp9_parser.ParseNextFrame(&frame_header, &allocate_size,
                                          &frame_decrypt_config),
                Vp9Parser::Result::kOk);

      EXPECT_EQ(frame_header.data.size(), buffers[j].size());
      // show_frame is 1 if and only if the frame is in the top spatial layer.
      EXPECT_EQ(frame_header.show_frame, j == i);
    }
  }
}

TEST(V4L2VP9HelpersTest, OverwriteShowFrameOOBRepro) {
  // Repro case from b/497431705:
  // buffer->size() = 12, spatial_layers = [16, 0]
  const size_t kBufferSize = 12;
  std::vector<uint8_t> tmp_buffer(kBufferSize, 0);
  // Set first byte to pass header checks: marker=0b10, profile=0,
  // show_existing=1
  tmp_buffer[0] = 0x88;

  auto decoder_buffer = DecoderBuffer::CopyFrom(tmp_buffer);
  std::vector<uint32_t> frame_sizes = {16, 0};
  AppendSideData(*decoder_buffer, frame_sizes);

  // This should fail because the sum of frame_sizes (16) exceeds the
  // original buffer size (12), or because a frame size is 0.
  EXPECT_FALSE(AppendVP9SuperFrameIndex(decoder_buffer));
}

}  // namespace media
