// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mac/video_toolbox_h265_accelerator.h"

#include <memory>

#include "base/containers/span.h"
#include "media/base/media_util.h"
#include "media/gpu/codec_picture.h"
#include "media/parsers/h265_parser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::SaveArg;

namespace media {

namespace {

// Configuration from buck1080p60_hevc.mp4
constexpr uint8_t kVPS0[] = {0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60,
                             0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03,
                             0x00, 0x00, 0x03, 0x00, 0x7b, 0x95, 0x98, 0x09};
constexpr uint8_t kSPS0[] = {
    0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x7b, 0xa0, 0x03, 0xc0, 0x80,
    0x10, 0xe5, 0x96, 0x56, 0x69, 0x24, 0xca, 0xf0, 0x16, 0x9c, 0x20,
    0x00, 0x00, 0x03, 0x00, 0x20, 0x00, 0x00, 0x07, 0x81};
constexpr uint8_t kPPS0[] = {0x44, 0x01, 0xc1, 0x72, 0xb4, 0x62, 0x40};

// Configuration from bear-1280x720-hevc-10bit-hdr10.mp4
constexpr uint8_t kVPS1[] = {0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x02, 0x20,
                             0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03,
                             0x00, 0x00, 0x03, 0x00, 0x5d, 0x95, 0x98, 0x09};
constexpr uint8_t kSPS1[] = {
    0x42, 0x01, 0x01, 0x02, 0x20, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x03, 0x00, 0x5d, 0xa0, 0x02, 0x80, 0x80, 0x2d, 0x13,
    0x65, 0x95, 0x9a, 0x49, 0x32, 0xbc, 0x05, 0xa8, 0x48, 0x80, 0x4f, 0x08,
    0x00, 0x00, 0x1f, 0x48, 0x00, 0x03, 0xa9, 0x80, 0x40};
constexpr uint8_t kPPS1[] = {0x44, 0x01, 0xc1, 0x72, 0xb4, 0x62, 0x40};

constexpr uint8_t kSliceData[] = {0x02};

}  // namespace

class VideoToolboxH265AcceleratorTest : public testing::Test {
 public:
  VideoToolboxH265AcceleratorTest() = default;
  ~VideoToolboxH265AcceleratorTest() override = default;

 protected:
  MOCK_METHOD3(OnDecode,
               void(base::apple::ScopedCFTypeRef<CMSampleBufferRef>,
                    VideoToolboxDecompressionSessionMetadata,
                    scoped_refptr<CodecPicture>));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<CodecPicture>));

  std::unique_ptr<VideoToolboxH265Accelerator> accelerator_{
      std::make_unique<VideoToolboxH265Accelerator>(
          std::make_unique<NullMediaLog>(),
          base::BindRepeating(&VideoToolboxH265AcceleratorTest::OnDecode,
                              base::Unretained(this)),
          base::BindRepeating(&VideoToolboxH265AcceleratorTest::OnOutput,
                              base::Unretained(this)))};
};

TEST_F(VideoToolboxH265AcceleratorTest, Construct) {}

TEST_F(VideoToolboxH265AcceleratorTest, DecodeOne) {
  scoped_refptr<H265Picture> pic = accelerator_->CreateH265Picture();
  H265VPS vps;
  H265SPS sps;
  H265PPS pps;
  H265SliceHeader slice_hdr;
  H265Picture::Vector ref_pic_list;
  std::vector<SubsampleEntry> subsamples;

  // Decode frame.
  accelerator_->ProcessVPS(&vps, base::make_span(kVPS0));
  accelerator_->ProcessSPS(&sps, base::make_span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::make_span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, &slice_hdr, ref_pic_list,
                                    ref_pic_list, ref_pic_list, ref_pic_list,
                                    pic);
  accelerator_->SubmitSlice(&sps, &pps, &slice_hdr, ref_pic_list, ref_pic_list,
                            ref_pic_list, ref_pic_list, ref_pic_list, pic,
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

TEST_F(VideoToolboxH265AcceleratorTest, DecodeTwo) {
  scoped_refptr<H265Picture> pic0 = accelerator_->CreateH265Picture();
  scoped_refptr<H265Picture> pic1 = accelerator_->CreateH265Picture();
  H265VPS vps;
  H265SPS sps;
  H265PPS pps;
  H265SliceHeader slice_hdr;
  H265Picture::Vector ref_pic_list;
  std::vector<SubsampleEntry> subsamples;

  // First frame.
  accelerator_->ProcessVPS(&vps, base::make_span(kVPS0));
  accelerator_->ProcessSPS(&sps, base::make_span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::make_span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, &slice_hdr, ref_pic_list,
                                    ref_pic_list, ref_pic_list, ref_pic_list,
                                    pic0);
  accelerator_->SubmitSlice(&sps, &pps, &slice_hdr, ref_pic_list, ref_pic_list,
                            ref_pic_list, ref_pic_list, ref_pic_list, pic0,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample0));
  accelerator_->SubmitDecode(pic0);

  // Second frame.
  accelerator_->ProcessVPS(&vps, base::make_span(kVPS0));
  accelerator_->ProcessSPS(&sps, base::make_span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::make_span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, &slice_hdr, ref_pic_list,
                                    ref_pic_list, ref_pic_list, ref_pic_list,
                                    pic1);
  accelerator_->SubmitSlice(&sps, &pps, &slice_hdr, ref_pic_list, ref_pic_list,
                            ref_pic_list, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample1));
  accelerator_->SubmitDecode(pic1);

  // The two samples should have the same configuration.
  EXPECT_EQ(CMSampleBufferGetFormatDescription(sample0.get()),
            CMSampleBufferGetFormatDescription(sample1.get()));
}

TEST_F(VideoToolboxH265AcceleratorTest, DecodeTwo_Reset) {
  scoped_refptr<H265Picture> pic0 = accelerator_->CreateH265Picture();
  scoped_refptr<H265Picture> pic1 = accelerator_->CreateH265Picture();
  H265VPS vps;
  H265SPS sps;
  H265PPS pps;
  H265SliceHeader slice_hdr;
  H265Picture::Vector ref_pic_list;
  std::vector<SubsampleEntry> subsamples;

  // First frame.
  accelerator_->ProcessVPS(&vps, base::make_span(kVPS0));
  accelerator_->ProcessSPS(&sps, base::make_span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::make_span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, &slice_hdr, ref_pic_list,
                                    ref_pic_list, ref_pic_list, ref_pic_list,
                                    pic0);
  accelerator_->SubmitSlice(&sps, &pps, &slice_hdr, ref_pic_list, ref_pic_list,
                            ref_pic_list, ref_pic_list, ref_pic_list, pic0,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample0));
  accelerator_->SubmitDecode(pic0);

  // Reset.
  accelerator_->Reset();

  // Second frame.
  accelerator_->ProcessVPS(&vps, base::make_span(kVPS0));
  accelerator_->ProcessSPS(&sps, base::make_span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::make_span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, &slice_hdr, ref_pic_list,
                                    ref_pic_list, ref_pic_list, ref_pic_list,
                                    pic1);
  accelerator_->SubmitSlice(&sps, &pps, &slice_hdr, ref_pic_list, ref_pic_list,
                            ref_pic_list, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample1));
  accelerator_->SubmitDecode(pic1);

  // The accelerator should have made a new configuration. (Technically it
  // should be fine to reuse the old one because the parameter sets did not
  // change.)
  EXPECT_NE(CMSampleBufferGetFormatDescription(sample0.get()),
            CMSampleBufferGetFormatDescription(sample1.get()));
}

TEST_F(VideoToolboxH265AcceleratorTest, DecodeTwo_ConfigChange) {
  scoped_refptr<H265Picture> pic0 = accelerator_->CreateH265Picture();
  scoped_refptr<H265Picture> pic1 = accelerator_->CreateH265Picture();
  H265VPS vps;
  H265SPS sps;
  H265PPS pps;
  H265SliceHeader slice_hdr;
  H265Picture::Vector ref_pic_list;
  std::vector<SubsampleEntry> subsamples;

  // First frame.
  accelerator_->ProcessVPS(&vps, base::make_span(kVPS0));
  accelerator_->ProcessSPS(&sps, base::make_span(kSPS0));
  accelerator_->ProcessPPS(&pps, base::make_span(kPPS0));
  accelerator_->SubmitFrameMetadata(&sps, &pps, &slice_hdr, ref_pic_list,
                                    ref_pic_list, ref_pic_list, ref_pic_list,
                                    pic0);
  accelerator_->SubmitSlice(&sps, &pps, &slice_hdr, ref_pic_list, ref_pic_list,
                            ref_pic_list, ref_pic_list, ref_pic_list, pic0,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample0;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample0));
  accelerator_->SubmitDecode(pic0);

  // Second frame.
  accelerator_->ProcessVPS(&vps, base::make_span(kVPS1));
  accelerator_->ProcessSPS(&sps, base::make_span(kSPS1));
  accelerator_->ProcessPPS(&pps, base::make_span(kPPS1));
  accelerator_->SubmitFrameMetadata(&sps, &pps, &slice_hdr, ref_pic_list,
                                    ref_pic_list, ref_pic_list, ref_pic_list,
                                    pic1);
  accelerator_->SubmitSlice(&sps, &pps, &slice_hdr, ref_pic_list, ref_pic_list,
                            ref_pic_list, ref_pic_list, ref_pic_list, pic1,
                            kSliceData, sizeof(kSliceData), subsamples);

  // Save the resulting sample.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample1;
  EXPECT_CALL(*this, OnDecode(_, _, _)).WillOnce(SaveArg<0>(&sample1));
  accelerator_->SubmitDecode(pic1);

  // The two samples should still have the same configurations.
  EXPECT_EQ(CMSampleBufferGetFormatDescription(sample0.get()),
            CMSampleBufferGetFormatDescription(sample1.get()));
}

}  // namespace media
