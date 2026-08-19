// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_h264_accelerator.h"

#include <memory>

#include "base/containers/span.h"
#include "media/base/media_util.h"
#include "media/gpu/codec_picture.h"
#include "media/parsers/h264_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::SaveArg;

namespace media {

namespace {

// Configuration from buck180p30.mp4
constexpr uint8_t kSPS0[] = {0x67, 0x64, 0x00, 0x28, 0xac, 0xd1, 0x00,
                             0x78, 0x02, 0x27, 0xe5, 0xc0, 0x44, 0x00,
                             0x00, 0x03, 0x00, 0x04, 0x00, 0x00, 0x03,
                             0x00, 0xf0, 0x3c, 0x60, 0xc4, 0x48};
constexpr uint8_t kPPS0[] = {0x68, 0xeb, 0xef, 0x2c};

// Configuration from bbb-320x240-2video-2audio.mp4
constexpr uint8_t kSPS1[] = {0x67, 0x64, 0x00, 0x0d, 0xac, 0xd9, 0x41,
                             0x41, 0xfb, 0x0e, 0x10, 0x00, 0x00, 0x03,
                             0x00, 0x10, 0x00, 0x00, 0x03, 0x03, 0xc0,
                             0xf1, 0x42, 0x99, 0x60};
constexpr uint8_t kPPS1[] = {0x68, 0xeb, 0xe0, 0xa4, 0xb2, 0x2c};

constexpr uint8_t kSliceData[] = {0x02};

}  // namespace

class VideoToolboxH264AcceleratorTest : public testing::Test {
 public:
  VideoToolboxH264AcceleratorTest() = default;
  ~VideoToolboxH264AcceleratorTest() override = default;

 protected:
  MOCK_METHOD3(OnDecode,
               void(base::apple::ScopedCFTypeRef<CMSampleBufferRef>,
                    VideoToolboxDecompressionSessionMetadata,
                    scoped_refptr<CodecPicture>));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<CodecPicture>));

  std::unique_ptr<VideoToolboxH264Accelerator> accelerator_{
      std::make_unique<VideoToolboxH264Accelerator>(
          std::make_unique<NullMediaLog>(),
          base::BindRepeating(&VideoToolboxH264AcceleratorTest::OnDecode,
                              base::Unretained(this)),
          base::BindRepeating(&VideoToolboxH264AcceleratorTest::OnOutput,
                              base::Unretained(this)))};
};

TEST_F(VideoToolboxH264AcceleratorTest, Construct) {}

TEST_F(VideoToolboxH264AcceleratorTest, DecodeOne) {
  scoped_refptr<H264Picture> pic = accelerator_->CreateH264Picture();
  H264SPS sps;
  H264PPS pps;
  H264DPB dpb;
  H264Picture::Vector ref_pic_list;
  H264SliceHeader slice_hdr;
  std::vector<SubsampleEntry> subsamples;

  // Decode frame.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample));
  accelerator_->SubmitDecode(pic);

  // Verify sample.
  CMBlockBufferRef buf = CMSampleBufferGetDataBuffer(sample.get());
  std::vector<uint8_t> data(CMBlockBufferGetDataLength(buf));
  CMBlockBufferCopyDataBytes(buf, 0, CMBlockBufferGetDataLength(buf),
                             data.data());
  EXPECT_THAT(data, ElementsAre(0x00, 0x00, 0x00, 0x01,  // length
                                0x02                     // kSliceData
                                ));

  // Check that OutputPicture() works.
  EXPECT_CALL(*this, OnOutput(_));
  accelerator_->OutputPicture(pic);
}

TEST_F(VideoToolboxH264AcceleratorTest, DecodeTwo) {
  scoped_refptr<H264Picture> pic0 = accelerator_->CreateH264Picture();
  scoped_refptr<H264Picture> pic1 = accelerator_->CreateH264Picture();
  H264SPS sps;
  H264PPS pps;
  H264DPB dpb;
  H264Picture::Vector ref_pic_list;
  H264SliceHeader slice_hdr;
  std::vector<SubsampleEntry> subsamples;

  // First frame.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic0);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic0,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample0));
  accelerator_->SubmitDecode(pic0);

  // Second frame.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic1);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample1));
  accelerator_->SubmitDecode(pic1);

  // The two samples should have the same configuration.
  EXPECT_EQ(CMSampleBufferGetFormatDescription(sample0.get()),
            CMSampleBufferGetFormatDescription(sample1.get()));
}

TEST_F(VideoToolboxH264AcceleratorTest, DecodeTwo_Reset) {
  scoped_refptr<H264Picture> pic0 = accelerator_->CreateH264Picture();
  scoped_refptr<H264Picture> pic1 = accelerator_->CreateH264Picture();
  H264SPS sps;
  H264PPS pps;
  H264DPB dpb;
  H264Picture::Vector ref_pic_list;
  H264SliceHeader slice_hdr;
  std::vector<SubsampleEntry> subsamples;

  // First frame.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic0);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic0,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample0));
  accelerator_->SubmitDecode(pic0);

  // Reset.
  accelerator_->Reset();

  // Second frame.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic1);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample1));
  accelerator_->SubmitDecode(pic1);

  // The two samples should have different configurations.
  EXPECT_NE(CMSampleBufferGetFormatDescription(sample0.get()),
            CMSampleBufferGetFormatDescription(sample1.get()));
}

TEST_F(VideoToolboxH264AcceleratorTest, DecodeTwo_ConfigChange) {
  scoped_refptr<H264Picture> pic0 = accelerator_->CreateH264Picture();
  scoped_refptr<H264Picture> pic1 = accelerator_->CreateH264Picture();
  pic1->idr = true;
  H264SPS sps;
  H264PPS pps;
  H264DPB dpb;
  H264Picture::Vector ref_pic_list;
  H264SliceHeader slice_hdr;
  std::vector<SubsampleEntry> subsamples;

  // First frame.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic0);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic0,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample0));
  accelerator_->SubmitDecode(pic0);

  // Second frame.
  accelerator_->ProcessSPS(&sps, base::span(kSPS1));
  accelerator_->ProcessPPS(&pps, base::span(kPPS1));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic1);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample1));
  accelerator_->SubmitDecode(pic1);

  // The two samples should have different configurations because pic1 is an
  // IDR.
  EXPECT_NE(CMSampleBufferGetFormatDescription(sample0.get()),
            CMSampleBufferGetFormatDescription(sample1.get()));
}

TEST_F(VideoToolboxH264AcceleratorTest, DecodeTwo_PPSChange_NonIDR) {
  scoped_refptr<H264Picture> pic0 = accelerator_->CreateH264Picture();
  scoped_refptr<H264Picture> pic1 = accelerator_->CreateH264Picture();
  pic1->idr = false;
  H264SPS sps;
  H264PPS pps;
  H264DPB dpb;
  H264Picture::Vector ref_pic_list;
  H264SliceHeader slice_hdr;
  std::vector<SubsampleEntry> subsamples;

  // First frame.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic0);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic0,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample0));
  accelerator_->SubmitDecode(pic0);

  // Second frame with new PPS.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS1));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic1);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample1));
  accelerator_->SubmitDecode(pic1);

  // The format description should remain the same.
  EXPECT_EQ(CMSampleBufferGetFormatDescription(sample0.get()),
            CMSampleBufferGetFormatDescription(sample1.get()));

  // The sample buffer should contain the new PPS in-band before slice data.
  CMBlockBufferRef buf = CMSampleBufferGetDataBuffer(sample1.get());
  std::vector<uint8_t> data(CMBlockBufferGetDataLength(buf));
  CMBlockBufferCopyDataBytes(buf, 0, CMBlockBufferGetDataLength(buf),
                             data.data());
  EXPECT_THAT(data, ElementsAre(0x00, 0x00, 0x00, 0x06,  // length of PPS1
                                0x68, 0xeb, 0xe0, 0xa4, 0xb2, 0x2c,  // kPPS1
                                0x00, 0x00, 0x00, 0x01,  // length of slice
                                0x02                     // kSliceData
                                ));
}

TEST_F(VideoToolboxH264AcceleratorTest, DecodeTwo_PPSChange_IDR) {
  scoped_refptr<H264Picture> pic0 = accelerator_->CreateH264Picture();
  scoped_refptr<H264Picture> pic1 = accelerator_->CreateH264Picture();
  pic1->idr = true;
  H264SPS sps;
  H264PPS pps;
  H264DPB dpb;
  H264Picture::Vector ref_pic_list;
  H264SliceHeader slice_hdr;
  std::vector<SubsampleEntry> subsamples;

  // First frame.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic0);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic0,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample0));
  accelerator_->SubmitDecode(pic0);

  // Second frame with new PPS on IDR keyframe.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::span(kPPS1));
  accelerator_->SubmitFrameMetadata(&sps, &pps, dpb, ref_pic_list, ref_pic_list,
                                    ref_pic_list, pic1);
  accelerator_->SubmitSlice(&pps, &slice_hdr, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample1));
  accelerator_->SubmitDecode(pic1);

  // The format description should change on IDR keyframe.
  EXPECT_NE(CMSampleBufferGetFormatDescription(sample0.get()),
            CMSampleBufferGetFormatDescription(sample1.get()));

  // The sample buffer should contain only slice data, as the PPS was updated
  // in the format description.
  CMBlockBufferRef buf = CMSampleBufferGetDataBuffer(sample1.get());
  std::vector<uint8_t> data(CMBlockBufferGetDataLength(buf));
  CMBlockBufferCopyDataBytes(buf, 0, CMBlockBufferGetDataLength(buf),
                             data.data());
  EXPECT_THAT(data, ElementsAre(0x00, 0x00, 0x00, 0x01,  // length of slice
                                0x02                     // kSliceData
                                ));
}

TEST_F(VideoToolboxH264AcceleratorTest, DecodeTwo_MultiSlice_PPS) {
  scoped_refptr<H264Picture> pic0 = accelerator_->CreateH264Picture();
  scoped_refptr<H264Picture> pic1 = accelerator_->CreateH264Picture();
  H264SPS sps;
  H264PPS pps0;
  pps0.pic_parameter_set_id = 0;
  pps0.seq_parameter_set_id = 0;
  H264PPS pps1;
  pps1.pic_parameter_set_id = 1;
  pps1.seq_parameter_set_id = 0;
  H264DPB dpb;
  H264Picture::Vector ref_pic_list;
  H264SliceHeader slice_hdr;
  std::vector<SubsampleEntry> subsamples;

  // First frame with PPS0.
  accelerator_->ProcessSPS(&sps, base::span(kSPS0));
  accelerator_->ProcessPPS(&pps0, base::span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps0, dpb, ref_pic_list,
                                    ref_pic_list, ref_pic_list, pic0);
  accelerator_->SubmitSlice(&pps0, &slice_hdr, ref_pic_list, ref_pic_list, pic0,
                            kSliceData, sizeof(kSliceData), subsamples);

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample0));
  accelerator_->SubmitDecode(pic0);

  // Second frame with two slices: first slice uses PPS0, second slice uses
  // PPS1.
  accelerator_->ProcessPPS(&pps1, base::span(kPPS1));
  accelerator_->SubmitFrameMetadata(&sps, &pps0, dpb, ref_pic_list,
                                    ref_pic_list, ref_pic_list, pic1);
  accelerator_->SubmitSlice(&pps0, &slice_hdr, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);
  accelerator_->SubmitSlice(&pps1, &slice_hdr, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);

  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample1));
  accelerator_->SubmitDecode(pic1);

  // The format description should remain the same.
  EXPECT_EQ(CMSampleBufferGetFormatDescription(sample0.get()),
            CMSampleBufferGetFormatDescription(sample1.get()));

  // The sample buffer should contain PPS1 in-band followed by slice 0 and
  // slice 1.
  CMBlockBufferRef buf = CMSampleBufferGetDataBuffer(sample1.get());
  std::vector<uint8_t> data(CMBlockBufferGetDataLength(buf));
  CMBlockBufferCopyDataBytes(buf, 0, CMBlockBufferGetDataLength(buf),
                             data.data());
  EXPECT_THAT(data, ElementsAre(0x00, 0x00, 0x00, 0x06,  // length of PPS1
                                0x68, 0xeb, 0xe0, 0xa4, 0xb2, 0x2c,  // kPPS1
                                0x00, 0x00, 0x00, 0x01,  // length of slice 0
                                0x02,                    // kSliceData
                                0x00, 0x00, 0x00, 0x01,  // length of slice 1
                                0x02                     // kSliceData
                                ));
}

}  // namespace media
