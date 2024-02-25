// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "media/base/media_util.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/mac/video_toolbox_vp9_accelerator.h"
#include "media/gpu/vp9_picture.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::SaveArg;

namespace media {

class VideoToolboxVP9AcceleratorTest : public testing::Test {
 public:
  VideoToolboxVP9AcceleratorTest() = default;
  ~VideoToolboxVP9AcceleratorTest() override = default;

 protected:
  MOCK_METHOD3(OnDecode,
               void(base::apple::ScopedCFTypeRef<CMSampleBufferRef>,
                    VideoToolboxDecompressionSessionMetadata,
                    scoped_refptr<CodecPicture>));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<CodecPicture>));

  std::unique_ptr<VideoToolboxVP9Accelerator> accelerator_{
      std::make_unique<VideoToolboxVP9Accelerator>(
          std::make_unique<NullMediaLog>(),
          std::nullopt,
          base::BindRepeating(&VideoToolboxVP9AcceleratorTest::OnDecode,
                              base::Unretained(this)),
          base::BindRepeating(&VideoToolboxVP9AcceleratorTest::OnOutput,
                              base::Unretained(this)))};
};

TEST_F(VideoToolboxVP9AcceleratorTest, Construct) {}

TEST_F(VideoToolboxVP9AcceleratorTest, DecodeRaw) {
  const Vp9SegmentationParams segm_params = {0};
  const Vp9LoopFilterParams lf_params = {0};
  const Vp9ReferenceFrameVector reference_frames;

  constexpr uint8_t frame_data[] = {0x01};

  scoped_refptr<VP9Picture> pic = accelerator_->CreateVP9Picture();
  pic->frame_hdr = std::make_unique<Vp9FrameHeader>();
  pic->frame_hdr->show_frame = true;
  pic->frame_hdr->data = frame_data;
  pic->frame_hdr->frame_size = 1;

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample));
  EXPECT_CALL(*this, OnOutput(_));
  accelerator_->SubmitDecode(pic, segm_params, lf_params, reference_frames);
  accelerator_->OutputPicture(pic);

  // Verify `sample`.
  CMBlockBufferRef buf = CMSampleBufferGetDataBuffer(sample.get());
  std::vector<uint8_t> data(CMBlockBufferGetDataLength(buf));
  CMBlockBufferCopyDataBytes(buf, 0, CMBlockBufferGetDataLength(buf),
                             data.data());
  EXPECT_THAT(data, ElementsAre(0x01));
}

TEST_F(VideoToolboxVP9AcceleratorTest, DecodeSuperframe) {
  const Vp9SegmentationParams segm_params = {0};
  const Vp9LoopFilterParams lf_params = {0};
  const Vp9ReferenceFrameVector reference_frames;

  constexpr uint8_t frame_data1[] = {0x01};
  constexpr uint8_t frame_data2[] = {0x02};

  scoped_refptr<VP9Picture> pic1 = accelerator_->CreateVP9Picture();
  pic1->frame_hdr = std::make_unique<Vp9FrameHeader>();
  pic1->frame_hdr->data = frame_data1;
  pic1->frame_hdr->frame_size = sizeof(frame_data1);

  scoped_refptr<VP9Picture> pic2 = accelerator_->CreateVP9Picture();
  pic2->frame_hdr = std::make_unique<Vp9FrameHeader>();
  pic2->frame_hdr->show_existing_frame = true;
  pic2->frame_hdr->data = frame_data2;
  pic2->frame_hdr->frame_size = sizeof(frame_data2);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample));
  EXPECT_CALL(*this, OnOutput(_));
  accelerator_->SubmitDecode(pic1, segm_params, lf_params, reference_frames);
  accelerator_->OutputPicture(pic2);

  // Verify `sample`.
  CMBlockBufferRef buf = CMSampleBufferGetDataBuffer(sample.get());
  std::vector<uint8_t> data(CMBlockBufferGetDataLength(buf));
  CMBlockBufferCopyDataBytes(buf, 0, CMBlockBufferGetDataLength(buf),
                             data.data());
  EXPECT_THAT(data, ElementsAre(0x01,                    // frame_data1
                                0x02,                    // frame_data2
                                0b11011001,              // header
                                0x01, 0x00, 0x00, 0x00,  // frame_data1 size
                                0x01, 0x00, 0x00, 0x00,  // frame_data2 size
                                0b11011001               // header
                                ));
}

}  // namespace media
