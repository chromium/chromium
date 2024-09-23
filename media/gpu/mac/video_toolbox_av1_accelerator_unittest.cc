// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "media/base/media_util.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/mac/video_toolbox_av1_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAreArray;
using testing::SaveArg;

namespace media {

class VideoToolboxAV1AcceleratorTest : public testing::Test {
 public:
  VideoToolboxAV1AcceleratorTest() = default;
  ~VideoToolboxAV1AcceleratorTest() override = default;

 protected:
  MOCK_METHOD3(OnDecode,
               void(base::apple::ScopedCFTypeRef<CMSampleBufferRef>,
                    VideoToolboxDecompressionSessionMetadata,
                    scoped_refptr<CodecPicture>));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<CodecPicture>));

  std::unique_ptr<VideoToolboxAV1Accelerator> accelerator_{
      std::make_unique<VideoToolboxAV1Accelerator>(
          std::make_unique<NullMediaLog>(),
          std::nullopt,
          base::BindRepeating(&VideoToolboxAV1AcceleratorTest::OnDecode,
                              base::Unretained(this)),
          base::BindRepeating(&VideoToolboxAV1AcceleratorTest::OnOutput,
                              base::Unretained(this)))};
};

TEST_F(VideoToolboxAV1AcceleratorTest, Construct) {}

TEST_F(VideoToolboxAV1AcceleratorTest, DecodeRaw) {
  // Sequence Header OBU from bear-av1.webm.
  // A valid sequence header is required to extract the av1c.
  constexpr uint8_t frame_data[] = {0x0a, 0x0b, 0x00, 0x00, 0x00, 0x04, 0x3c,
                                    0xff, 0xbc, 0xfb, 0xf9, 0x80, 0x40};

  libgav1::ObuSequenceHeader sequence_header = {};
  sequence_header.profile = libgav1::kProfile0;
  sequence_header.color_config.bitdepth = 8;

  const AV1ReferenceFrameVector ref_frames;
  const libgav1::Vector<libgav1::TileBuffer> tile_buffers;

  scoped_refptr<AV1Picture> pic = accelerator_->CreateAV1Picture(false);
  pic->frame_header.width = 320;
  pic->frame_header.height = 240;
  pic->set_visible_rect(gfx::Rect(320, 240));

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample));
  EXPECT_CALL(*this, OnOutput(_));
  accelerator_->SetStream(base::make_span(frame_data), nullptr);
  accelerator_->SubmitDecode(*pic, sequence_header, ref_frames, tile_buffers,
                             base::make_span(frame_data));
  accelerator_->OutputPicture(*pic);

  // Verify `sample`.
  CMBlockBufferRef buf = CMSampleBufferGetDataBuffer(sample.get());
  std::vector<uint8_t> data(CMBlockBufferGetDataLength(buf));
  CMBlockBufferCopyDataBytes(buf, 0, CMBlockBufferGetDataLength(buf),
                             data.data());
  EXPECT_THAT(data, ElementsAreArray(frame_data));
}

TEST_F(VideoToolboxAV1AcceleratorTest, DecodeSuperframe) {
  // Sequence Header OBU from bear-av1.webm.
  // A valid sequence header is required to extract the av1c.
  constexpr uint8_t superframe_data[] = {0x0a, 0x0b, 0x00, 0x00, 0x00,
                                         0x04, 0x3c, 0xff, 0xbc, 0xfb,
                                         0xf9, 0x80, 0x40};

  libgav1::ObuSequenceHeader sequence_header = {};
  sequence_header.profile = libgav1::kProfile0;
  sequence_header.color_config.bitdepth = 8;

  const AV1ReferenceFrameVector ref_frames;
  const libgav1::Vector<libgav1::TileBuffer> tile_buffers;

  scoped_refptr<AV1Picture> pic1 = accelerator_->CreateAV1Picture(false);
  pic1->frame_header.width = 320;
  pic1->frame_header.height = 240;
  pic1->set_visible_rect(gfx::Rect(320, 240));

  scoped_refptr<AV1Picture> pic2 = accelerator_->CreateAV1Picture(false);
  pic2->frame_header.width = 320;
  pic2->frame_header.height = 240;
  pic2->set_visible_rect(gfx::Rect(320, 240));

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillRepeatedly(SaveArg<0>(&sample));
  EXPECT_CALL(*this, OnOutput(_)).Times(2);
  accelerator_->SetStream(base::make_span(superframe_data), nullptr);
  accelerator_->SubmitDecode(*pic1, sequence_header, ref_frames, tile_buffers,
                             base::make_span(superframe_data));
  accelerator_->SubmitDecode(*pic2, sequence_header, ref_frames, tile_buffers,
                             base::make_span(superframe_data));
  accelerator_->OutputPicture(*pic2);

  // Verify `sample`.
  CMBlockBufferRef buf = CMSampleBufferGetDataBuffer(sample.get());
  std::vector<uint8_t> data(CMBlockBufferGetDataLength(buf));
  CMBlockBufferCopyDataBytes(buf, 0, CMBlockBufferGetDataLength(buf),
                             data.data());
  // Once AV1Decoder splits frame data into frames, this will be a constructed
  // superframe. For now, we assume that the original data is already a
  // superframe.
  EXPECT_THAT(data, ElementsAreArray(superframe_data));

  // Submit `show_existing_frame` frame.
  constexpr uint8_t show_existing_frame_data[] = {0x01, 0x02, 0x03, 0x04};
  accelerator_->SetStream(base::make_span(show_existing_frame_data), nullptr);
  accelerator_->OutputPicture(*pic1);

  // Verify `sample`.
  CMBlockBufferRef buf2 = CMSampleBufferGetDataBuffer(sample.get());
  std::vector<uint8_t> data2(CMBlockBufferGetDataLength(buf2));
  CMBlockBufferCopyDataBytes(buf2, 0, CMBlockBufferGetDataLength(buf2),
                             data2.data());
  EXPECT_THAT(data2, ElementsAreArray(show_existing_frame_data));
}

}  // namespace media
