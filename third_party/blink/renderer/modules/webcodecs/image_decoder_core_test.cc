// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_core.h"

#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class ImageDecoderCoreTest : public testing::Test {
 public:
  ~ImageDecoderCoreTest() override = default;

 protected:
  std::unique_ptr<ImageDecoderCore> CreateDecoder(const char* file_name,
                                                  const char* mime_type) {
    auto data = ReadFile(file_name);
    DCHECK(data->size()) << "Missing file: " << file_name;
    return std::make_unique<ImageDecoderCore>(
        mime_type, std::move(data),
        /*data_complete=*/true, ColorBehavior::kTag, SkISize::MakeEmpty(),
        ImageDecoder::AnimationOption::kPreferAnimation);
  }

  scoped_refptr<SegmentReader> ReadFile(StringView file_name) {
    StringBuilder file_path;
    file_path.Append(test::BlinkWebTestsDir());
    file_path.Append('/');
    file_path.Append(file_name);
    std::optional<Vector<char>> data = test::ReadFromFile(file_path.ToString());
    CHECK(data);
    return SegmentReader::CreateFromSharedBuffer(
        SharedBuffer::Create(std::move(*data)));
  }
  test::TaskEnvironment task_environment_;
};

TEST_F(ImageDecoderCoreTest, InOrderDecodePreservesMemory) {
  constexpr char kImageType[] = "image/webp";
  auto decoder =
      CreateDecoder("images/resources/webp-animated-large.webp", kImageType);
  ASSERT_TRUE(decoder);

  const auto metadata = decoder->DecodeMetadata();
  EXPECT_EQ(metadata.frame_count, 8u);
  EXPECT_EQ(metadata.data_complete, true);

  // In order decoding should only preserve the most recent frames. Loop twice
  // to ensure looping doesn't trigger out of order decoding.
  base::AtomicFlag abort_flag;
  for (int j = 0; j < 2; ++j) {
    for (uint32_t i = 0; i < metadata.frame_count; ++i) {
      auto result =
          decoder->Decode(i, /*complete_frames_only=*/true, &abort_flag);
      EXPECT_TRUE(!!result->frame);

      // Only the current frame should be preserved.
      EXPECT_TRUE(decoder->FrameIsDecodedAtIndexForTesting(i));
      if (i >= 1)
        EXPECT_FALSE(decoder->FrameIsDecodedAtIndexForTesting(i - 1));
      if (i >= 2)
        EXPECT_FALSE(decoder->FrameIsDecodedAtIndexForTesting(i - 2));
    }
  }

  // Out of order decoding should stop eviction.
  decoder->Decode(metadata.frame_count / 2, /*complete_frames_only=*/true,
                  &abort_flag);

  for (uint32_t i = 0; i < metadata.frame_count; ++i) {
    auto result =
        decoder->Decode(i, /*complete_frames_only=*/true, &abort_flag);
    EXPECT_TRUE(!!result->frame);

    // All frames should be preserved.
    EXPECT_TRUE(decoder->FrameIsDecodedAtIndexForTesting(i));
    if (i >= 1)
      EXPECT_TRUE(decoder->FrameIsDecodedAtIndexForTesting(i - 1));
    if (i >= 2)
      EXPECT_TRUE(decoder->FrameIsDecodedAtIndexForTesting(i - 2));
  }
}

}  // namespace

}  // namespace blink
