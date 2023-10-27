// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/vp9_super_frame_bitstream_filter.h"

#include <CoreMedia/CoreMedia.h>

#include "media/base/media.h"
#include "media/base/test_data_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/filters/vp9_parser.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

#if BUILDFLAG(ENABLE_FFMPEG)

class VP9SuperFrameBitstreamFilterTest : public testing::Test {
 public:
  VP9SuperFrameBitstreamFilterTest()
      : parser_(/*parsing_compressed_header=*/false) {
    InitializeMediaLibrary();
  }

  ~VP9SuperFrameBitstreamFilterTest() override = default;

  void LoadTestData(const char* file_name) {
    buffer_ = ReadTestDataFile(file_name);
    ASSERT_TRUE(buffer_);

    // Initialize ffmpeg with the file data.
    protocol_ = std::make_unique<InMemoryUrlProtocol>(
        buffer_->data(), buffer_->data_size(), false);
    glue_ = std::make_unique<FFmpegGlue>(protocol_.get());
    ASSERT_TRUE(glue_->OpenContext());
  }

  scoped_refptr<DecoderBuffer> ReadPacket(int stream_index = 0) {
    AVPacket packet = {0};
    while (av_read_frame(glue_->format_context(), &packet) >= 0) {
      if (packet.stream_index == stream_index) {
        auto buffer = DecoderBuffer::CopyFrom(packet.data, packet.size);
        av_packet_unref(&packet);
        return buffer;
      }
      av_packet_unref(&packet);
    }
    return nullptr;
  }

  Vp9Parser::Result ParseNextFrame() {
    // Temporaries for the Vp9Parser.
    Vp9FrameHeader fhdr;
    gfx::Size coded_size;
    std::unique_ptr<DecryptConfig> null_config;
    return parser_.ParseNextFrame(&fhdr, &coded_size, &null_config);
  }

 protected:
  Vp9Parser parser_;

 private:
  scoped_refptr<DecoderBuffer> buffer_;
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  std::unique_ptr<FFmpegGlue> glue_;
};

TEST_F(VP9SuperFrameBitstreamFilterTest, Passthrough) {
  // This test file has no super frames.
  ASSERT_NO_FATAL_FAILURE(LoadTestData("bear-vp9.webm"));

  // Run through a few packets for good measure.
  VP9SuperFrameBitstreamFilter bsf;
  for (int i = 0; i < 16; ++i) {
    auto buffer = ReadPacket();
    EXPECT_TRUE(buffer->HasOneRef());

    // Passthrough buffers should be zero-copy, so a ref should be added.
    bsf.EnqueueBuffer(buffer);
    EXPECT_FALSE(buffer->HasOneRef());

    auto cm_block = bsf.take_buffer();
    ASSERT_TRUE(cm_block);

    ASSERT_EQ(buffer->data_size(), CMBlockBufferGetDataLength(cm_block.get()));

    std::unique_ptr<uint8_t> block_data(new uint8_t[buffer->data_size()]);
    ASSERT_EQ(noErr,
              CMBlockBufferCopyDataBytes(cm_block.get(), 0, buffer->data_size(),
                                         block_data.get()));

    // Verify that the block is valid.
    parser_.SetStream(block_data.get(), buffer->data_size(), nullptr);
    EXPECT_EQ(Vp9Parser::kOk, ParseNextFrame());
    EXPECT_EQ(Vp9Parser::kEOStream, ParseNextFrame());

    // Releasing the block should bring our ref count back down.
    cm_block.reset();
    ASSERT_TRUE(buffer->HasOneRef());
  }
}

TEST_F(VP9SuperFrameBitstreamFilterTest, Superframe) {
  ASSERT_NO_FATAL_FAILURE(LoadTestData("buck-1280x720-vp9.webm"));

  VP9SuperFrameBitstreamFilter bsf;

  // The first packet in this file is not part of a super frame. We still need
  // to send it to the VP9 parser so that the superframe can reference it.
  auto buffer = ReadPacket();
  parser_.SetStream(buffer->data(), buffer->data_size(), nullptr);
  EXPECT_EQ(Vp9Parser::kOk, ParseNextFrame());
  bsf.EnqueueBuffer(std::move(buffer));
  ASSERT_TRUE(bsf.take_buffer());

  // The second and third belong to a super frame.
  buffer = ReadPacket();
  size_t total_size = buffer->data_size();
  bsf.EnqueueBuffer(std::move(buffer));
  ASSERT_FALSE(bsf.take_buffer());
  buffer = ReadPacket();
  total_size += buffer->data_size();
  bsf.EnqueueBuffer(std::move(buffer));

  auto cm_block = bsf.take_buffer();
  ASSERT_TRUE(cm_block);

  // Two marker bytes and 2x 16-bit sizes.
  const size_t kExpectedTotalSize = 1 + 2 + 2 + 1 + total_size;
  EXPECT_EQ(kExpectedTotalSize, CMBlockBufferGetDataLength(cm_block.get()));

  std::unique_ptr<uint8_t> block_data(new uint8_t[kExpectedTotalSize]);
  ASSERT_EQ(noErr,
            CMBlockBufferCopyDataBytes(cm_block.get(), 0, kExpectedTotalSize,
                                       block_data.get()));

  parser_.SetStream(block_data.get(), kExpectedTotalSize, nullptr);
  EXPECT_EQ(Vp9Parser::kOk, ParseNextFrame());
  EXPECT_EQ(Vp9Parser::kOk, ParseNextFrame());
  EXPECT_EQ(Vp9Parser::kEOStream, ParseNextFrame());
}

TEST_F(VP9SuperFrameBitstreamFilterTest, FlushPassthroughFrame) {
  ASSERT_NO_FATAL_FAILURE(LoadTestData("buck-1280x720-vp9.webm"));

  VP9SuperFrameBitstreamFilter bsf;

  // The first packet in this file is not part of a super frame.
  bsf.EnqueueBuffer(ReadPacket());
  ASSERT_TRUE(bsf.has_buffers_for_testing());
  bsf.Flush();
  ASSERT_FALSE(bsf.has_buffers_for_testing());
  ASSERT_FALSE(bsf.take_buffer());
}

TEST_F(VP9SuperFrameBitstreamFilterTest, FlushPartialSuperFrame) {
  ASSERT_NO_FATAL_FAILURE(LoadTestData("buck-1280x720-vp9.webm"));

  VP9SuperFrameBitstreamFilter bsf;

  // The first packet in this file is not part of a super frame.
  bsf.EnqueueBuffer(ReadPacket());
  ASSERT_TRUE(bsf.has_buffers_for_testing());
  ASSERT_TRUE(bsf.take_buffer());

  // The second and third belong to a super frame.
  bsf.EnqueueBuffer(ReadPacket());
  ASSERT_FALSE(bsf.take_buffer());
  ASSERT_TRUE(bsf.has_buffers_for_testing());

  bsf.Flush();
  ASSERT_FALSE(bsf.has_buffers_for_testing());
  ASSERT_FALSE(bsf.take_buffer());
}

#endif  // BUILDFLAG(ENABLE_FFMPEG)

}  // namespace media
