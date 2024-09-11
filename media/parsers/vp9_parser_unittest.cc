// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// For some sample vp9 test videos, $filename, there is a file of golden value
// of frame entropy, named $filename.context. These values are dumped from
// libvpx.
//
// The syntax of these context dump is described as follows.  For every
// frame, there are corresponding data in context file,
// 1. [initial] [current] [should_update=0], or
// 2. [initial] [current] [should_update=1] [update]
// The first two are expected frame entropy, fhdr->initial_frame_context and
// fhdr->frame_context.
// If |should_update| is true, it follows by the frame context to update.
#include "media/parsers/vp9_parser.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/parsers/ivf_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::TestWithParam;

namespace media {

namespace {

struct TestParams {
  const char* file_name;
  int profile;
  int bit_depth;
  size_t width;
  size_t height;
  bool frame_parallel_decoding_mode;
  int loop_filter_level;
  int quantization_base_index;
  size_t first_frame_header_size_bytes;
  enum Vp9InterpolationFilter filter;
  size_t second_frame_header_size_bytes;
  size_t second_frame_uncompressed_header_size_bytes;
};

const struct TestParams kTestParams[] = {
    {"test-25fps.vp9", 0, 8, 320, 240, true, 9, 65, 120,
     Vp9InterpolationFilter::EIGHTTAP, 48, 11},
    {"test-25fps.vp9_2", 2, 10, 320, 240, true, 8, 79, 115,
     Vp9InterpolationFilter::SWITCHABLE, 46, 10}};

const char kInitialIV[] = "aaaaaaaaaaaaaaaa";
const char kIVIncrementOne[] = "aaaaaaaaaaaaaaab";
const char kIVIncrementTwo[] = "aaaaaaaaaaaaaaac";
const char kKeyID[] = "key-id";

}  // anonymous namespace

class Vp9ParserTest : public TestWithParam<TestParams> {
 protected:
  Vp9ParserTest() = default;
  void TearDown() override {
    stream_.reset();
    vp9_parser_.reset();
    context_file_.Close();
  }

  void Initialize(std::string_view filename, bool parsing_compressed_header) {
    base::FilePath file_path = GetTestDataFilePath(filename);

    stream_ = std::make_unique<base::MemoryMappedFile>();
    ASSERT_TRUE(stream_->Initialize(file_path)) << "Couldn't open stream file: "
                                                << file_path.MaybeAsASCII();

    IvfFileHeader ivf_file_header;
    ASSERT_TRUE(ivf_parser_.Initialize(stream_->data(), stream_->length(),
                                       &ivf_file_header));
    ASSERT_EQ(ivf_file_header.fourcc, 0x30395056u);  // VP90

    vp9_parser_ = std::make_unique<Vp9Parser>(parsing_compressed_header);

    if (parsing_compressed_header) {
      base::FilePath context_path =
          GetTestDataFilePath(std::string(filename).append(".context"));
      context_file_.Initialize(context_path,
                               base::File::FLAG_OPEN | base::File::FLAG_READ);
      ASSERT_TRUE(context_file_.IsValid());
    }
  }

  bool ReadShouldContextUpdate() {
    char should_update;
    int read_num = context_file_.ReadAtCurrentPos(&should_update, 1);
    EXPECT_EQ(1, read_num);
    return should_update != 0;
  }

  void ReadContext(Vp9FrameContext* frame_context) {
    ASSERT_EQ(
        static_cast<int>(sizeof(*frame_context)),
        context_file_.ReadAtCurrentPos(reinterpret_cast<char*>(frame_context),
                                       sizeof(*frame_context)));
  }

  Vp9Parser::Result ParseNextFrame(struct Vp9FrameHeader* frame_hdr);
  void CheckSubsampleValues(
      const uint8_t* superframe,
      size_t framesize,
      std::unique_ptr<DecryptConfig> config,
      std::vector<std::unique_ptr<DecryptConfig>>& expected_split);

  const Vp9SegmentationParams& GetSegmentation() const {
    return vp9_parser_->context().segmentation();
  }

  const Vp9LoopFilterParams& GetLoopFilter() const {
    return vp9_parser_->context().loop_filter();
  }

  IvfParser ivf_parser_;
  std::unique_ptr<base::MemoryMappedFile> stream_;

  std::unique_ptr<Vp9Parser> vp9_parser_;
  base::File context_file_;
};

Vp9Parser::Result Vp9ParserTest::ParseNextFrame(Vp9FrameHeader* fhdr) {
  while (true) {
    std::unique_ptr<DecryptConfig> null_config;
    gfx::Size allocate_size;
    Vp9Parser::Result res =
        vp9_parser_->ParseNextFrame(fhdr, &allocate_size, &null_config);
    if (res == Vp9Parser::kEOStream) {
      IvfFrameHeader ivf_frame_header;
      const uint8_t* ivf_payload;

      if (!ivf_parser_.ParseNextFrame(&ivf_frame_header, &ivf_payload))
        return Vp9Parser::kEOStream;

      vp9_parser_->SetStream(ivf_payload, ivf_frame_header.frame_size,
                             nullptr);
      continue;
    }

    return res;
  }
}

void Vp9ParserTest::CheckSubsampleValues(
    const uint8_t* superframe,
    size_t framesize,
    std::unique_ptr<DecryptConfig> config,
    std::vector<std::unique_ptr<DecryptConfig>>& expected_split) {
  vp9_parser_->SetStream(superframe, framesize, std::move(config));
  for (auto& expected : expected_split) {
    std::unique_ptr<DecryptConfig> actual =
        vp9_parser_->NextFrameDecryptContextForTesting();
    EXPECT_EQ(actual->iv(), expected->iv());
    EXPECT_EQ(actual->subsamples().size(), expected->subsamples().size());
  }
}

uint8_t make_marker_byte(bool is_superframe, const uint8_t frame_count) {
  DCHECK_LE(frame_count, 8);
  const uint8_t superframe_marker_byte =
      // superframe marker byte
      // marker (0b110) encoded at bits 6, 7, 8
      // or non-superframe marker (0b111)
      (0xE0 & ((is_superframe ? 0x06 : 0x07) << 5)) |
      // magnitude - 1 encoded at bits 4, 5
      (0x18 & (0x00 << 3)) |
      // frame count - 2 encoded at bits 1, 2, 3
      (0x07 & (frame_count - 1));
  return superframe_marker_byte;
}

// ┌───────────────────┬────────────────────┐
// │ frame 1           │ frame 2            │
// ┝━━━━━━━━━━━━━━━━━━━┿━━━━━━━━━━━━━━━━━━━━┥
// │ clear1 | cipher 1 │ clear 2 | cipher 2 │
// └───────────────────┴────────────────────┘
TEST_F(Vp9ParserTest, AlignedFrameSubsampleParsing) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  const uint8_t superframe_marker_byte = make_marker_byte(true, 2);
  const uint8_t kSuperframe[] = {
      // First frame; 32 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Second frame; 32 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Superframe marker goes before and after frame index.
      superframe_marker_byte,
      // First frame length (magnitude 1)
      0x20,
      // Second frame length (magnigude 1)
      0x20,
      // marker again.
      superframe_marker_byte};

  std::vector<std::unique_ptr<DecryptConfig>> expected;
  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kInitialIV, {SubsampleEntry(16, 16)}, std::nullopt));

  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kIVIncrementOne, {SubsampleEntry(16, 16)}, std::nullopt));

  CheckSubsampleValues(
      kSuperframe, sizeof(kSuperframe),
      DecryptConfig::CreateCencConfig(
          kKeyID, kInitialIV, {SubsampleEntry(16, 16), SubsampleEntry(16, 16)}),
      expected);
}

// ┌───────────────────┬────────────────────┐
// │ frame 1           │ frame 2            │
// ┝━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━━━━━━━━━━━━┥
// │ clear1                  | cipher 1     │
// └────────────────────────────────────────┘
TEST_F(Vp9ParserTest, UnalignedFrameSubsampleParsing) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  const uint8_t superframe_marker_byte = make_marker_byte(true, 2);
  const uint8_t kSuperframe[] = {
      // First frame; 32 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Second frame; 32 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Superframe marker goes before and after frame index.
      superframe_marker_byte,
      // First frame length (magnitude 1)
      0x20,
      // Second frame length (magnigude 1)
      0x20,
      // marker again.
      superframe_marker_byte};

  std::vector<std::unique_ptr<DecryptConfig>> expected;
  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kInitialIV, {SubsampleEntry(32, 0)}, std::nullopt));

  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kInitialIV, {SubsampleEntry(16, 16)}, std::nullopt));

  CheckSubsampleValues(kSuperframe, sizeof(kSuperframe),
                       DecryptConfig::CreateCencConfig(
                           kKeyID, kInitialIV, {SubsampleEntry(48, 16)}),
                       expected);
}

// ┌─────────────────────────┬────────────────────┐
// │ frame 1                 │ frame 2            │
// ┝━━━━━━━━━━━━━━━━━━━┯━━━━━┷━━━━━━━━━━━━━━━━━━━━┥
// │ clear1 | cipher 1 │ clear 2       | cipher 2 │
// └───────────────────┴──────────────────────────┘
TEST_F(Vp9ParserTest, ClearSectionRollsOverSubsampleParsing) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  const uint8_t superframe_marker_byte = make_marker_byte(true, 2);
  const uint8_t kSuperframe[] = {
      // First frame; 48 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Second frame; 32 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Superframe marker goes before and after frame index.
      superframe_marker_byte,
      // First frame length (magnitude 1)
      0x30,
      // Second frame length (magnigude 1)
      0x20,
      // marker again.
      superframe_marker_byte};

  std::vector<std::unique_ptr<DecryptConfig>> expected;
  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kInitialIV, {SubsampleEntry(16, 16), SubsampleEntry(16, 0)},
      std::nullopt));

  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kIVIncrementOne, {SubsampleEntry(16, 16)}, std::nullopt));

  CheckSubsampleValues(
      kSuperframe, sizeof(kSuperframe),
      DecryptConfig::CreateCencConfig(
          kKeyID, kInitialIV, {SubsampleEntry(16, 16), SubsampleEntry(32, 16)}),
      expected);
}

// ┌────────────────────────────────────────┬────────────────────┐
// │ frame 1                                │ frame 2            │
// ┝━━━━━━━━━━━━━━━━━━━┯━━━━━━━━━━━━━━━━━━━━┿━━━━━━━━━━━━━━━━━━━━┥
// │ clear1 | cipher 1 │ clear 2 | cipher 2 │ clear 3 | cipher 3 │
// └───────────────────┴────────────────────┴────────────────────┘
TEST_F(Vp9ParserTest, FirstFrame2xSubsampleParsing) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  const uint8_t superframe_marker_byte = make_marker_byte(true, 2);
  const uint8_t kSuperframe[] = {
      // First frame; 64 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      // Second frame; 32 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Superframe marker goes before and after frame index.
      superframe_marker_byte,
      // First frame length (magnitude 1)
      0x40,
      // Second frame length (magnigude 1)
      0x20,
      // marker again.
      superframe_marker_byte};

  std::vector<std::unique_ptr<DecryptConfig>> expected;
  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kInitialIV, {SubsampleEntry(16, 16), SubsampleEntry(16, 16)},
      std::nullopt));

  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kIVIncrementTwo, {SubsampleEntry(16, 16)}, std::nullopt));

  CheckSubsampleValues(kSuperframe, sizeof(kSuperframe),
                       DecryptConfig::CreateCencConfig(
                           kKeyID, kInitialIV,
                           {SubsampleEntry(16, 16), SubsampleEntry(16, 16),
                            SubsampleEntry(16, 16)}),
                       expected);
}

// ┌─────────────────────────────────────────────┬───────────────┐
// │ frame 1                                     │ frame 2       │
// ┝━━━━━━━━━━━━━━━━━━━┯━━━━━━━━━━━━━━━━━━━━┯━━━━┷━━━━━━━━━━━━━━━┥
// │ clear1 | cipher 1 │ clear 2 | cipher 2 │ clear 3 | cipher 3 │
// └───────────────────┴────────────────────┴────────────────────┘
TEST_F(Vp9ParserTest, UnalignedBigFrameSubsampleParsing) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  const uint8_t superframe_marker_byte = make_marker_byte(true, 2);
  const uint8_t kSuperframe[] = {
      // First frame; 72 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Second frame; 32 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Superframe marker goes before and after frame index.
      superframe_marker_byte,
      // First frame length (magnitude 1)
      0x48,
      // Second frame length (magnigude 1)
      0x20,
      // marker again.
      superframe_marker_byte};

  std::vector<std::unique_ptr<DecryptConfig>> expected;
  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kInitialIV,
      {SubsampleEntry(16, 16), SubsampleEntry(16, 16), SubsampleEntry(8, 0)},
      std::nullopt));

  expected.push_back(DecryptConfig::CreateCbcsConfig(
      kKeyID, kIVIncrementTwo, {SubsampleEntry(16, 16)}, std::nullopt));

  CheckSubsampleValues(
      kSuperframe, sizeof(kSuperframe),
      DecryptConfig::CreateCencConfig(kKeyID, kInitialIV,
                                      {
                                          SubsampleEntry(16, 16),
                                          SubsampleEntry(16, 16),
                                          SubsampleEntry(24, 16),
                                      }),
      expected);
}

// ┌───────────────────┬────────────────────┐
// │ frame 1           │ frame 2            │
// ┝━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━━━━━━━━━━━━┥
// │ clear1      | cipher 1                 │
// └────────────────────────────────────────┘
TEST_F(Vp9ParserTest, UnalignedInvalidSubsampleParsing) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  const uint8_t superframe_marker_byte = make_marker_byte(true, 2);
  const uint8_t kSuperframe[] = {
      // First frame; 32 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Second frame; 32 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Superframe marker goes before and after frame index.
      superframe_marker_byte,
      // First frame length (magnitude 1)
      0x20,
      // Second frame length (magnigude 1)
      0x20,
      // marker again.
      superframe_marker_byte};

  vp9_parser_->SetStream(kSuperframe, sizeof(kSuperframe),
                         DecryptConfig::CreateCencConfig(
                             kKeyID, kInitialIV, {SubsampleEntry(16, 32)}));

  ASSERT_EQ(vp9_parser_->NextFrameDecryptContextForTesting().get(), nullptr);
}

// ┌─────────────────────────────────────┬─────────┐
// │ single frame in superframe          │ marker  │
// ┝━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━┥
// │ clear1 = 0  | cipher 1                        │
// └───────────────────────────────────────────────┘
TEST_F(Vp9ParserTest, CipherBytesCoverSuperframeMarkerSubsampleParsing) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  const uint8_t superframe_marker_byte = make_marker_byte(false, 1);
  const uint8_t kSuperframe[] = {
      // First frame; 44 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Superframe marker goes before and after frame index.
      superframe_marker_byte,
      // First frame length (magnitude 1)
      0x20,
      // Second frame length (magnigude 1)
      0x20,
      // marker again.
      superframe_marker_byte};

  vp9_parser_->SetStream(kSuperframe, sizeof(kSuperframe),
                         DecryptConfig::CreateCencConfig(
                             kKeyID, kInitialIV, {SubsampleEntry(0, 48)}));

  std::unique_ptr<DecryptConfig> actual =
      vp9_parser_->NextFrameDecryptContextForTesting();

  EXPECT_EQ(actual->iv(), kInitialIV);
  EXPECT_EQ(actual->subsamples().size(), 1lu);
}

// ┌─────────────────────────────────────┬─────────┐
// │ single frame in superframe          │ marker  │
// ┝━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┷━━━━━━━━━┥
// │ clear1                                        │
// └───────────────────────────────────────────────┘
TEST_F(Vp9ParserTest, ClearBytesCoverSuperframeMarkerSubsampleParsing) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  const uint8_t superframe_marker_byte = make_marker_byte(false, 1);
  const uint8_t kSuperframe[] = {
      // First frame; 44 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Superframe marker goes before and after frame index.
      superframe_marker_byte,
      // First frame length (magnitude 1)
      0x20,
      // Second frame length (magnigude 1)
      0x20,
      // marker again.
      superframe_marker_byte};

  vp9_parser_->SetStream(kSuperframe, sizeof(kSuperframe),
                         DecryptConfig::CreateCencConfig(
                             kKeyID, kInitialIV, {SubsampleEntry(48, 0)}));

  std::unique_ptr<DecryptConfig> actual =
      vp9_parser_->NextFrameDecryptContextForTesting();

  EXPECT_EQ(actual->iv(), kInitialIV);
  EXPECT_EQ(actual->subsamples().size(), 1lu);
}

// ┌─────────────────────────────────────┬─────────┐
// │ single frame in superframe          │ marker  │
// ┝━━━━━━━━━━━━━━━━━━━━┯━━━━━━━━━━━━━━━━┷━━━━━━━━━┥
// │ clear 1 | cipher 1 │ clear 2                  │
// └────────────────────┴──────────────────────────┘
TEST_F(Vp9ParserTest, SecondClearSubsampleSuperframeMarkerSubsampleParsing) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  const uint8_t superframe_marker_byte = make_marker_byte(false, 1);
  const uint8_t kSuperframe[] = {
      // First frame; 44 bytes.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      // Superframe marker goes before and after frame index.
      superframe_marker_byte,
      // First frame length (magnitude 1)
      0x20,
      // Second frame length (magnigude 1)
      0x20,
      // marker again.
      superframe_marker_byte};

  vp9_parser_->SetStream(
      kSuperframe, sizeof(kSuperframe),
      DecryptConfig::CreateCencConfig(kKeyID, kInitialIV,
                                      {
                                          SubsampleEntry(16, 16),
                                          SubsampleEntry(16, 0),
                                      }));

  std::unique_ptr<DecryptConfig> actual =
      vp9_parser_->NextFrameDecryptContextForTesting();

  EXPECT_EQ(actual->iv(), kInitialIV);
  EXPECT_EQ(actual->subsamples().size(), 2lu);
}

TEST_F(Vp9ParserTest, TestIncrementIV) {
  vp9_parser_ = std::make_unique<Vp9Parser>(false);

  std::vector<std::tuple<char const*, uint32_t, char const*>> input_output = {
      {"--------aaaaaaaa", 1, "--------aaaaaaab"},
      {"--------aaaaaaa\377", 1, "--------aaaaaab\0"},
      {"--------aaaaaaa\377", 2, "--------aaaaaab\1"},
      {"--------\377\377\377\377\377\377\377\377", 2,
       "--------\0\0\0\0\0\0\0\1"}};

  for (auto& testcase : input_output) {
    EXPECT_EQ(
        vp9_parser_->IncrementIVForTesting(
            std::string(std::get<0>(testcase), 16), std::get<1>(testcase)),
        std::string(std::get<2>(testcase), 16));
  }
}

TEST_F(Vp9ParserTest, StreamFileParsingWithoutCompressedHeader) {
  Initialize("test-25fps.vp9", /*parsing_compressed_header=*/false);

  // Number of frames in the test stream to be parsed.
  const int num_expected_frames = 269;
  int num_parsed_frames = 0;

  // Allow to parse twice as many frames in order to detect any extra frames
  // parsed.
  while (num_parsed_frames < num_expected_frames * 2) {
    Vp9FrameHeader fhdr;
    if (ParseNextFrame(&fhdr) != Vp9Parser::kOk)
      break;

    ++num_parsed_frames;
  }

  DVLOG(1) << "Number of successfully parsed frames before EOS: "
           << num_parsed_frames;

  EXPECT_EQ(num_expected_frames, num_parsed_frames);
}

TEST_P(Vp9ParserTest, VerifyFirstFrame) {
  Initialize(GetParam().file_name, /*parsing_compressed_header=*/false);
  Vp9FrameHeader fhdr;

  ASSERT_EQ(Vp9Parser::kOk, ParseNextFrame(&fhdr));

  EXPECT_EQ(GetParam().profile, fhdr.profile);
  EXPECT_FALSE(fhdr.show_existing_frame);
  EXPECT_EQ(Vp9FrameHeader::KEYFRAME, fhdr.frame_type);
  EXPECT_TRUE(fhdr.show_frame);
  EXPECT_FALSE(fhdr.error_resilient_mode);

  EXPECT_EQ(GetParam().bit_depth, fhdr.bit_depth);
  EXPECT_EQ(Vp9ColorSpace::UNKNOWN, fhdr.color_space);
  EXPECT_FALSE(fhdr.color_range);
  EXPECT_EQ(1, fhdr.subsampling_x);
  EXPECT_EQ(1, fhdr.subsampling_y);

  EXPECT_EQ(GetParam().width, fhdr.frame_width);
  EXPECT_EQ(GetParam().height, fhdr.frame_height);
  EXPECT_EQ(GetParam().width, fhdr.render_width);
  EXPECT_EQ(GetParam().height, fhdr.render_height);

  EXPECT_TRUE(fhdr.refresh_frame_context);
  EXPECT_EQ(GetParam().frame_parallel_decoding_mode,
            fhdr.frame_parallel_decoding_mode);
  EXPECT_EQ(0, fhdr.frame_context_idx_to_save_probs);

  const Vp9LoopFilterParams& lf = GetLoopFilter();
  EXPECT_EQ(GetParam().loop_filter_level, lf.level);
  EXPECT_EQ(0, lf.sharpness);
  EXPECT_TRUE(lf.delta_enabled);
  EXPECT_TRUE(lf.delta_update);
  EXPECT_TRUE(lf.update_ref_deltas[0]);
  EXPECT_EQ(1, lf.ref_deltas[0]);
  EXPECT_EQ(-1, lf.ref_deltas[2]);
  EXPECT_EQ(-1, lf.ref_deltas[3]);

  const Vp9QuantizationParams& qp = fhdr.quant_params;
  EXPECT_EQ(GetParam().quantization_base_index, qp.base_q_idx);
  EXPECT_FALSE(qp.delta_q_y_dc);
  EXPECT_FALSE(qp.delta_q_uv_dc);
  EXPECT_FALSE(qp.delta_q_uv_ac);
  EXPECT_FALSE(qp.IsLossless());

  const Vp9SegmentationParams& seg = GetSegmentation();
  EXPECT_FALSE(seg.enabled);

  EXPECT_EQ(0, fhdr.tile_cols_log2);
  EXPECT_EQ(0, fhdr.tile_rows_log2);

  EXPECT_EQ(GetParam().first_frame_header_size_bytes,
            fhdr.header_size_in_bytes);
  EXPECT_EQ(18u, fhdr.uncompressed_header_size);

  // Now verify the second frame in the file which should be INTERFRAME.
  ASSERT_EQ(Vp9Parser::kOk, ParseNextFrame(&fhdr));

  EXPECT_EQ(GetParam().bit_depth, fhdr.bit_depth);
  EXPECT_EQ(Vp9ColorSpace::UNKNOWN, fhdr.color_space);
  EXPECT_EQ(Vp9FrameHeader::INTERFRAME, fhdr.frame_type);
  EXPECT_FALSE(fhdr.show_frame);
  EXPECT_FALSE(fhdr.intra_only);
  EXPECT_FALSE(fhdr.reset_frame_context);
  EXPECT_TRUE(fhdr.RefreshFlag(2));
  EXPECT_EQ(0, fhdr.ref_frame_idx[0]);
  EXPECT_EQ(1, fhdr.ref_frame_idx[1]);
  EXPECT_EQ(2, fhdr.ref_frame_idx[2]);
  EXPECT_TRUE(fhdr.allow_high_precision_mv);
  EXPECT_EQ(GetParam().filter, fhdr.interpolation_filter);

  EXPECT_EQ(GetParam().second_frame_header_size_bytes,
            fhdr.header_size_in_bytes);
  EXPECT_EQ(GetParam().second_frame_uncompressed_header_size_bytes,
            fhdr.uncompressed_header_size);
}

INSTANTIATE_TEST_SUITE_P(All, Vp9ParserTest, ::testing::ValuesIn(kTestParams));

TEST_F(Vp9ParserTest, CheckColorSpace) {
  Vp9FrameHeader fhdr;
  EXPECT_FALSE(fhdr.GetColorSpace().IsSpecified());
  fhdr.color_space = Vp9ColorSpace::BT_709;
  EXPECT_EQ(VideoColorSpace::REC709(), fhdr.GetColorSpace());
  fhdr.color_space = Vp9ColorSpace::BT_601;
  EXPECT_EQ(VideoColorSpace::REC601(), fhdr.GetColorSpace());
}

}  // namespace media
