// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/ffmpeg_glue.h"

#include <stdint.h>

#include <memory>

#include "base/check.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "media/base/container_names.h"
#include "media/base/media_switches.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_deleters.h"
#include "media/filters/in_memory_url_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace media {

class MockProtocol : public FFmpegURLProtocol {
 public:
  MockProtocol() = default;

  MockProtocol(const MockProtocol&) = delete;
  MockProtocol& operator=(const MockProtocol&) = delete;

  virtual ~MockProtocol() = default;

  MOCK_METHOD2(Read, int(int size, uint8_t* data));
  MOCK_METHOD1(GetPosition, bool(int64_t* position_out));
  MOCK_METHOD1(SetPosition, bool(int64_t position));
  MOCK_METHOD1(GetSize, bool(int64_t* size_out));
  MOCK_METHOD0(IsStreaming, bool());
};

class FFmpegGlueTest : public ::testing::Test {
 public:
  FFmpegGlueTest() : protocol_(std::make_unique<StrictMock<MockProtocol>>()) {
    // IsStreaming() is called when opening.
    EXPECT_CALL(*protocol_.get(), IsStreaming()).WillOnce(Return(true));
    glue_ = std::make_unique<FFmpegGlue>(protocol_.get());
    CHECK(glue_->format_context());
    CHECK(glue_->format_context()->pb);
  }

  FFmpegGlueTest(const FFmpegGlueTest&) = delete;
  FFmpegGlueTest& operator=(const FFmpegGlueTest&) = delete;

  ~FFmpegGlueTest() override {
    // Ensure |glue_| and |protocol_| are still alive.
    CHECK(glue_.get());
    CHECK(protocol_.get());

    // |protocol_| should outlive |glue_|, so ensure it's destructed first.
    glue_.reset();
  }

  int ReadPacket(int size, uint8_t* data) {
    return glue_->format_context()->pb->read_packet(protocol_.get(), data,
                                                    size);
  }

  int64_t Seek(int64_t offset, int whence) {
    return glue_->format_context()->pb->seek(protocol_.get(), offset, whence);
  }

 protected:
  std::unique_ptr<FFmpegGlue> glue_;
  std::unique_ptr<StrictMock<MockProtocol>> protocol_;
};

class FFmpegGlueDestructionTest : public ::testing::Test {
 public:
  FFmpegGlueDestructionTest() = default;

  void Initialize(const char* filename) {
    data_ = ReadTestDataFile(filename);
    protocol_ = std::make_unique<InMemoryUrlProtocol>(data_->data(),
                                                      data_->size(), false);
    glue_ = std::make_unique<FFmpegGlue>(protocol_.get());
    CHECK(glue_->format_context());
    CHECK(glue_->format_context()->pb);
  }

  FFmpegGlueDestructionTest(const FFmpegGlueDestructionTest&) = delete;
  FFmpegGlueDestructionTest& operator=(const FFmpegGlueDestructionTest&) =
      delete;

  ~FFmpegGlueDestructionTest() override {
    // Ensure Initialize() was called.
    CHECK(glue_.get());
    CHECK(protocol_.get());

    // |glue_| should be destroyed before |protocol_|.
    glue_.reset();

    // |protocol_| should be destroyed before |data_|.
    protocol_.reset();
    data_.reset();
  }

 protected:
  std::unique_ptr<FFmpegGlue> glue_;

 private:
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  scoped_refptr<DecoderBuffer> data_;
};

// Tests that ensure we are using the correct AVInputFormat name given by ffmpeg
// for supported containers.
class FFmpegGlueContainerTest : public FFmpegGlueDestructionTest {
 public:
  FFmpegGlueContainerTest() = default;

  FFmpegGlueContainerTest(const FFmpegGlueContainerTest&) = delete;
  FFmpegGlueContainerTest& operator=(const FFmpegGlueContainerTest&) = delete;

  ~FFmpegGlueContainerTest() override = default;

 protected:
  void InitializeAndOpen(const char* filename) {
    Initialize(filename);
    ASSERT_TRUE(glue_->OpenContext());
  }

  void ExpectContainer(container_names::MediaContainerName container) {
    histogram_tester_.ExpectUniqueSample("Media.DetectedContainer", container,
                                         1);
  }

 private:
  base::HistogramTester histogram_tester_;
};

// Ensure writing has been disabled.
TEST_F(FFmpegGlueTest, Write) {
  ASSERT_FALSE(glue_->format_context()->pb->write_packet);
  ASSERT_FALSE(glue_->format_context()->pb->write_flag);
}

// Test both successful and unsuccessful reads pass through correctly.
TEST_F(FFmpegGlueTest, Read) {
  const int kBufferSize = 16;
  uint8_t buffer[kBufferSize];

  // Reads are for the most part straight-through calls to Read().
  InSequence s;
  EXPECT_CALL(*protocol_, Read(0, buffer))
      .WillOnce(Return(0));
  EXPECT_CALL(*protocol_, Read(kBufferSize, buffer))
      .WillOnce(Return(kBufferSize));
  EXPECT_CALL(*protocol_, Read(kBufferSize, buffer))
      .WillOnce(Return(AVERROR(EIO)));

  EXPECT_EQ(0, ReadPacket(0, buffer));
  EXPECT_EQ(kBufferSize, ReadPacket(kBufferSize, buffer));
  EXPECT_EQ(AVERROR(EIO), ReadPacket(kBufferSize, buffer));
}

// Test a variety of seek operations.
TEST_F(FFmpegGlueTest, Seek) {
  // SEEK_SET should be a straight-through call to SetPosition(), which when
  // successful will return the result from GetPosition().
  InSequence s;
  EXPECT_CALL(*protocol_, SetPosition(-16))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, SetPosition(16))
      .WillOnce(Return(true));
  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgPointee<0>(8), Return(true)));

  EXPECT_EQ(AVERROR(EIO), Seek(-16, SEEK_SET));
  EXPECT_EQ(8, Seek(16, SEEK_SET));

  // SEEK_CUR should call GetPosition() first, and if it succeeds add the offset
  // to the result then call SetPosition()+GetPosition().
  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgPointee<0>(8), Return(true)));
  EXPECT_CALL(*protocol_, SetPosition(16))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgPointee<0>(8), Return(true)));
  EXPECT_CALL(*protocol_, SetPosition(16))
      .WillOnce(Return(true));
  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)));

  EXPECT_EQ(AVERROR(EIO), Seek(8, SEEK_CUR));
  EXPECT_EQ(AVERROR(EIO), Seek(8, SEEK_CUR));
  EXPECT_EQ(16, Seek(8, SEEK_CUR));

  // SEEK_END should call GetSize() first, and if it succeeds add the offset
  // to the result then call SetPosition()+GetPosition().
  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)));
  EXPECT_CALL(*protocol_, SetPosition(8))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)));
  EXPECT_CALL(*protocol_, SetPosition(8))
      .WillOnce(Return(true));
  EXPECT_CALL(*protocol_, GetPosition(_))
      .WillOnce(DoAll(SetArgPointee<0>(8), Return(true)));

  EXPECT_EQ(AVERROR(EIO), Seek(-8, SEEK_END));
  EXPECT_EQ(AVERROR(EIO), Seek(-8, SEEK_END));
  EXPECT_EQ(8, Seek(-8, SEEK_END));

  // AVSEEK_SIZE should be a straight-through call to GetSize().
  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(Return(false));

  EXPECT_CALL(*protocol_, GetSize(_))
      .WillOnce(DoAll(SetArgPointee<0>(16), Return(true)));

  EXPECT_EQ(AVERROR(EIO), Seek(0, AVSEEK_SIZE));
  EXPECT_EQ(16, Seek(0, AVSEEK_SIZE));
}

// Ensure destruction release the appropriate resources when OpenContext() is
// never called.
TEST_F(FFmpegGlueDestructionTest, WithoutOpen) {
  Initialize("ten_byte_file");
}

// Ensure destruction releases the appropriate resources when
// avformat_open_input() fails.
TEST_F(FFmpegGlueDestructionTest, WithOpenFailure) {
  Initialize("ten_byte_file");
  ASSERT_FALSE(glue_->OpenContext());
}

// Ensure destruction release the appropriate resources when OpenContext() is
// called, but no streams have been opened.
TEST_F(FFmpegGlueDestructionTest, WithOpenNoStreams) {
  Initialize("no_streams.webm");
  ASSERT_TRUE(glue_->OpenContext());
}

// Ensure destruction release the appropriate resources when OpenContext() is
// called and streams exist.
TEST_F(FFmpegGlueDestructionTest, WithOpenWithStreams) {
  Initialize("bear-320x240.webm");
  ASSERT_TRUE(glue_->OpenContext());
}

// Ensure destruction release the appropriate resources when OpenContext() is
// called and streams have been opened. This now requires user of FFmpegGlue to
// ensure any allocated AVCodecContext is closed prior to ~FFmpegGlue().
TEST_F(FFmpegGlueDestructionTest, WithOpenWithOpenStreams) {
  Initialize("bear-320x240.webm");
  ASSERT_TRUE(glue_->OpenContext());
  ASSERT_GT(glue_->format_context()->nb_streams, 0u);

  // Use ScopedPtrAVFreeContext to ensure |context| is closed, and use scoping
  // and ordering to ensure |context| is destructed before |glue_|.
  // Pick the audio stream (1) so this works when the ffmpeg video decoders are
  // disabled.
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> context(
      AVStreamToAVCodecContext(glue_->format_context()->streams[1]));
  ASSERT_NE(nullptr, context.get());
  ASSERT_EQ(0, avcodec_open2(context.get(),
                             avcodec_find_decoder(context->codec_id), nullptr));
}

TEST_F(FFmpegGlueContainerTest, OGG) {
  InitializeAndOpen("sfx.ogg");
  ExpectContainer(container_names::MediaContainerName::kContainerOgg);
}

TEST_F(FFmpegGlueContainerTest, WEBM) {
  InitializeAndOpen("sfx-opus-441.webm");
  ExpectContainer(container_names::MediaContainerName::kContainerWEBM);
}

TEST_F(FFmpegGlueContainerTest, FLAC) {
  InitializeAndOpen("sfx.flac");
  ExpectContainer(container_names::MediaContainerName::kContainerFLAC);
}

TEST_F(FFmpegGlueContainerTest, WAV) {
  InitializeAndOpen("sfx_s16le.wav");
  ExpectContainer(container_names::MediaContainerName::kContainerWAV);
}

TEST_F(FFmpegGlueContainerTest, MP3) {
  InitializeAndOpen("sfx.mp3");
  ExpectContainer(container_names::MediaContainerName::kContainerMP3);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
TEST_F(FFmpegGlueContainerTest, MOV) {
  InitializeAndOpen("sfx.m4a");
  ExpectContainer(container_names::MediaContainerName::kContainerMOV);
}

TEST_F(FFmpegGlueContainerTest, AAC) {
  InitializeAndOpen("sfx.adts");
  ExpectContainer(container_names::MediaContainerName::kContainerAAC);
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

// Probe something unsupported to ensure we fall back to the our internal guess.
TEST_F(FFmpegGlueContainerTest, FLV) {
  Initialize("bear.flv");
  ASSERT_FALSE(glue_->OpenContext());
  ExpectContainer(container_names::MediaContainerName::kContainerFLV);
}

}  // namespace media
