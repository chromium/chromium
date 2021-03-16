// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"

#include "base/memory/ref_counted.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/blink/renderer/platform/webrtc/testing/mock_webrtc_video_frame_adapter_shared_resources.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

TEST(ScaledBufferSizeTest, CroppingIsRelative) {
  const WebRtcVideoFrameAdapter::ScaledBufferSize k720p(
      gfx::Rect(0, 0, 1280, 720), gfx::Size(1280, 720));

  // Crop away a 100 pixel border.
  const auto cropped_full_scale =
      k720p.CropAndScale(100, 100, 1080, 520, 1080, 520);
  EXPECT_EQ(cropped_full_scale.visible_rect.x(), 100);
  EXPECT_EQ(cropped_full_scale.visible_rect.y(), 100);
  EXPECT_EQ(cropped_full_scale.visible_rect.width(), 1080);
  EXPECT_EQ(cropped_full_scale.visible_rect.height(), 520);
  EXPECT_EQ(cropped_full_scale.natural_size.width(), 1080);
  EXPECT_EQ(cropped_full_scale.natural_size.height(), 520);

  // Applying the same size again should be a NO-OP.
  const auto cropped_full_scale2 =
      cropped_full_scale.CropAndScale(0, 0, 1080, 520, 1080, 520);
  EXPECT_TRUE(cropped_full_scale2 == cropped_full_scale);

  // Cropping again is relative to the current crop. Crop on crop.
  const auto second_cropped_full_size =
      cropped_full_scale.CropAndScale(100, 100, 880, 320, 880, 320);
  EXPECT_EQ(second_cropped_full_size.visible_rect.x(), 200);
  EXPECT_EQ(second_cropped_full_size.visible_rect.y(), 200);
  EXPECT_EQ(second_cropped_full_size.visible_rect.width(), 880);
  EXPECT_EQ(second_cropped_full_size.visible_rect.height(), 320);
  EXPECT_EQ(second_cropped_full_size.natural_size.width(), 880);
  EXPECT_EQ(second_cropped_full_size.natural_size.height(), 320);

  // Applying the same size again should be a NO-OP.
  const auto second_cropped_full_size2 =
      second_cropped_full_size.CropAndScale(0, 0, 880, 320, 880, 320);
  EXPECT_TRUE(second_cropped_full_size2 == second_cropped_full_size);

  // Cropping again is relative to the current crop. Crop on crop on crop.
  const auto third_cropped_full_size =
      second_cropped_full_size.CropAndScale(100, 100, 680, 120, 680, 120);
  EXPECT_EQ(third_cropped_full_size.visible_rect.x(), 300);
  EXPECT_EQ(third_cropped_full_size.visible_rect.y(), 300);
  EXPECT_EQ(third_cropped_full_size.visible_rect.width(), 680);
  EXPECT_EQ(third_cropped_full_size.visible_rect.height(), 120);
  EXPECT_EQ(third_cropped_full_size.natural_size.width(), 680);
  EXPECT_EQ(third_cropped_full_size.natural_size.height(), 120);
}

TEST(ScaledBufferSizeTest, ScalingIsRelative) {
  const WebRtcVideoFrameAdapter::ScaledBufferSize k720p(
      gfx::Rect(0, 0, 1280, 720), gfx::Size(1280, 720));

  // Scale down by 2x.
  const auto no_crop_half_size = k720p.CropAndScale(0, 0, 1280, 720, 640, 360);
  EXPECT_EQ(no_crop_half_size.visible_rect.x(), 0);
  EXPECT_EQ(no_crop_half_size.visible_rect.y(), 0);
  EXPECT_EQ(no_crop_half_size.visible_rect.width(), 1280);
  EXPECT_EQ(no_crop_half_size.visible_rect.height(), 720);
  EXPECT_EQ(no_crop_half_size.natural_size.width(), 640);
  EXPECT_EQ(no_crop_half_size.natural_size.height(), 360);

  // Applying the same size again should be a NO-OP.
  const auto no_crop_half_size2 =
      no_crop_half_size.CropAndScale(0, 0, 640, 360, 640, 360);
  EXPECT_TRUE(no_crop_half_size2 == no_crop_half_size);

  // Scaling again is relative to the current scale. Half-size on half-size.
  const auto no_crop_quarter_size =
      no_crop_half_size.CropAndScale(0, 0, 640, 360, 320, 180);
  EXPECT_EQ(no_crop_quarter_size.visible_rect.x(), 0);
  EXPECT_EQ(no_crop_quarter_size.visible_rect.y(), 0);
  EXPECT_EQ(no_crop_quarter_size.visible_rect.width(), 1280);
  EXPECT_EQ(no_crop_quarter_size.visible_rect.height(), 720);
  EXPECT_EQ(no_crop_quarter_size.natural_size.width(), 320);
  EXPECT_EQ(no_crop_quarter_size.natural_size.height(), 180);

  // Applying the same size again should be a NO-OP.
  const auto no_crop_quarter_size2 =
      no_crop_quarter_size.CropAndScale(0, 0, 320, 180, 320, 180);
  EXPECT_TRUE(no_crop_quarter_size2 == no_crop_quarter_size);

  // Scaling again is relative to the current scale.
  // Half-size on half-size on half-size.
  const auto no_crop_eighths_size =
      no_crop_quarter_size.CropAndScale(0, 0, 320, 180, 160, 90);
  EXPECT_EQ(no_crop_eighths_size.visible_rect.x(), 0);
  EXPECT_EQ(no_crop_eighths_size.visible_rect.y(), 0);
  EXPECT_EQ(no_crop_eighths_size.visible_rect.width(), 1280);
  EXPECT_EQ(no_crop_eighths_size.visible_rect.height(), 720);
  EXPECT_EQ(no_crop_eighths_size.natural_size.width(), 160);
  EXPECT_EQ(no_crop_eighths_size.natural_size.height(), 90);
}

TEST(ScaledBufferSizeTest, CroppingAndScalingIsRelative) {
  const WebRtcVideoFrameAdapter::ScaledBufferSize k720p(
      gfx::Rect(0, 0, 1280, 720), gfx::Size(1280, 720));

  // Crop away a 100 pixel border and downscale by 2x.
  const auto crop_and_scale1 =
      k720p.CropAndScale(100, 100, 1080, 520, 540, 260);
  EXPECT_EQ(crop_and_scale1.visible_rect.x(), 100);
  EXPECT_EQ(crop_and_scale1.visible_rect.y(), 100);
  EXPECT_EQ(crop_and_scale1.visible_rect.width(), 1080);
  EXPECT_EQ(crop_and_scale1.visible_rect.height(), 520);
  EXPECT_EQ(crop_and_scale1.natural_size.width(), 540);
  EXPECT_EQ(crop_and_scale1.natural_size.height(), 260);

  // Cropping some more at the new scale without further downscale.
  const auto crop_and_scale2 =
      crop_and_scale1.CropAndScale(50, 50, 440, 160, 440, 160);
  // The delta offset is magnified due to scale. Offset = 100*1 + 50*2.
  EXPECT_EQ(crop_and_scale2.visible_rect.x(), 200);
  EXPECT_EQ(crop_and_scale2.visible_rect.y(), 200);
  EXPECT_EQ(crop_and_scale2.visible_rect.width(), 880);
  EXPECT_EQ(crop_and_scale2.visible_rect.height(), 320);
  EXPECT_EQ(crop_and_scale2.natural_size.width(), 440);
  EXPECT_EQ(crop_and_scale2.natural_size.height(), 160);

  // Scaling some more without further cropping.
  const auto crop_and_scale3 =
      crop_and_scale2.CropAndScale(0, 0, 440, 160, 220, 80);
  EXPECT_EQ(crop_and_scale3.visible_rect.x(), 200);
  EXPECT_EQ(crop_and_scale3.visible_rect.y(), 200);
  EXPECT_EQ(crop_and_scale3.visible_rect.width(), 880);
  EXPECT_EQ(crop_and_scale3.visible_rect.height(), 320);
  EXPECT_EQ(crop_and_scale3.natural_size.width(), 220);
  EXPECT_EQ(crop_and_scale3.natural_size.height(), 80);
}

TEST(WebRtcVideoFrameAdapterTest, MapFullFrameIsZeroCopy) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);

  // The strictness of the mock ensures zero copy.
  scoped_refptr<MockSharedResources> resources =
      new testing::StrictMock<MockSharedResources>();

  auto frame_720p = CreateTestFrame(kSize720p, kRect720p, kSize720p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);

  rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
          resources));

  // Mapping produces a frame of the correct size.
  auto mapped_frame = multi_buffer->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_frame->width(), kSize720p.width());
  EXPECT_EQ(mapped_frame->height(), kSize720p.height());
  // The mapping above should be backed by |frame_720p|.
  auto adapted_frame = multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kRect720p, kSize720p));
  EXPECT_EQ(adapted_frame, frame_720p);
}

TEST(WebRtcVideoFrameAdapterTest,
     MapScaledFrameCreatesNewFrameWhenNotPreScaled) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);
  const gfx::Size kSize360p(640, 360);

  // Because the size we are going to request does not the frame we expect one
  // CreateFrame() to happen.
  scoped_refptr<MockSharedResources> resources =
      new testing::StrictMock<MockSharedResources>();
  EXPECT_CALL(*resources, CreateFrame)
      .WillOnce(testing::Invoke(
          [](media::VideoPixelFormat format, const gfx::Size& coded_size,
             const gfx::Rect& visible_rect, const gfx::Size& natural_size,
             base::TimeDelta timestamp) {
            return CreateTestFrame(coded_size, visible_rect, natural_size,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   format);
          }));

  auto frame_720p = CreateTestFrame(kSize720p, kRect720p, kSize720p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);

  rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
          resources));

  auto scaled_frame =
      multi_buffer->Scale(kSize360p.width(), kSize360p.height());

  // Mapping produces a frame of the correct size.
  auto mapped_frame = scaled_frame->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_frame->width(), kSize360p.width());
  EXPECT_EQ(mapped_frame->height(), kSize360p.height());
  // The mapping above should be backed by a frame that wraps |frame_720p|. We
  // can tell by looking at the coded size.
  auto adapted_frame = multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kRect720p, kSize360p));
  ASSERT_TRUE(adapted_frame);
  EXPECT_EQ(adapted_frame->coded_size(), frame_720p->coded_size());
}

TEST(WebRtcVideoFrameAdapterTest, MapScaledFrameUsesPreScaling) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);
  const gfx::Size kSize360p(640, 360);
  const gfx::Rect kRect360p(0, 0, 640, 360);

  // The strictness of the mock ensures no additional scaling.
  scoped_refptr<MockSharedResources> resources =
      new testing::StrictMock<MockSharedResources>();

  auto frame_720p = CreateTestFrame(kSize720p, kRect720p, kSize720p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  auto frame_360p = CreateTestFrame(kSize360p, kRect360p, kSize360p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);

  rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame_720p,
          std::vector<scoped_refptr<media::VideoFrame>>({frame_360p}),
          resources));

  auto scaled_frame =
      multi_buffer->Scale(kSize360p.width(), kSize360p.height());

  // Mapping produces a frame of the correct size.
  auto mapped_frame = scaled_frame->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_frame->width(), kSize360p.width());
  EXPECT_EQ(mapped_frame->height(), kSize360p.height());
  // The mapping above should be backed by |frame_360p|.
  auto adapted_frame = multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kRect720p, kSize360p));
  EXPECT_EQ(adapted_frame, frame_360p);
}

TEST(WebRtcVideoFrameAdapterTest, MapScaledFrameScalesFromClosestFrame) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);
  const gfx::Size kSize480p(853, 480);
  const gfx::Rect kRect480p(0, 0, 853, 480);
  const gfx::Size kSize360p(640, 360);
  const gfx::Rect kRect360p(0, 0, 640, 360);

  // A size in-between 480p and 360p.
  const gfx::Size kSize432p(768, 432);

  // Because the size we are going to request does not match any of the frames
  // we expect one CreateFrame() to happen.
  scoped_refptr<MockSharedResources> resources =
      new testing::StrictMock<MockSharedResources>();
  EXPECT_CALL(*resources, CreateFrame)
      .WillOnce(testing::Invoke(
          [](media::VideoPixelFormat format, const gfx::Size& coded_size,
             const gfx::Rect& visible_rect, const gfx::Size& natural_size,
             base::TimeDelta timestamp) {
            return CreateTestFrame(coded_size, visible_rect, natural_size,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   format);
          }));

  auto frame_720p = CreateTestFrame(kSize720p, kRect720p, kSize720p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  auto frame_480p = CreateTestFrame(kSize480p, kRect480p, kSize480p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  auto frame_360p = CreateTestFrame(kSize360p, kRect360p, kSize360p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);

  rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame_720p,
          std::vector<scoped_refptr<media::VideoFrame>>(
              {frame_480p, frame_360p}),
          resources));

  auto scaled_frame =
      multi_buffer->Scale(kSize432p.width(), kSize432p.height());

  // Mapping produces a frame of the correct size.
  auto mapped_frame = scaled_frame->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_frame->width(), kSize432p.width());
  EXPECT_EQ(mapped_frame->height(), kSize432p.height());
  // The mapping above should be backed by a frame that wraps |frame_480p|. We
  // can tell by looking at the coded size.
  auto adapted_frame = multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kRect720p, kSize432p));
  ASSERT_TRUE(adapted_frame);
  EXPECT_EQ(adapted_frame->coded_size(), frame_480p->coded_size());
}

TEST(WebRtcVideoFrameAdapterTest, CanApplyCropAndScale) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);
  const gfx::Size kSize360p(640, 360);
  const gfx::Rect kRect360p(0, 0, 640, 360);

  const gfx::Rect kCroppedRect1(20, 20, 1240, 680);
  const gfx::Rect kCroppedRect2(20, 20, 1200, 640);
  const gfx::Size kScaledSize2(1200 / 2, 640 / 2);

  // The strictness of the mock ensures zero copy.
  scoped_refptr<MockSharedResources> resources =
      new testing::StrictMock<MockSharedResources>();

  auto frame_720p = CreateTestFrame(kSize720p, kRect720p, kSize720p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  auto frame_360p = CreateTestFrame(kSize360p, kRect360p, kSize360p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);

  rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame_720p,
          std::vector<scoped_refptr<media::VideoFrame>>({frame_360p}),
          resources));

  // Apply initial cropping, keeping the same scale.
  auto cropped_frame1 = multi_buffer->CropAndScale(
      kCroppedRect1.x(), kCroppedRect1.y(), kCroppedRect1.width(),
      kCroppedRect1.height(), kCroppedRect1.width(), kCroppedRect1.height());

  // Mapping produces a frame of the correct size.
  auto mapped_cropped_frame1 = cropped_frame1->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_cropped_frame1->width(), kCroppedRect1.width());
  EXPECT_EQ(mapped_cropped_frame1->height(), kCroppedRect1.height());
  // The mapping above should be backed by a frame that wraps |frame_720p|. We
  // can tell by looking at the coded size.
  auto adapted_frame = multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(
          kCroppedRect1,
          gfx::Size(kCroppedRect1.width(), kCroppedRect1.height())));
  ASSERT_TRUE(adapted_frame);
  EXPECT_EQ(adapted_frame->coded_size(), frame_720p->coded_size());

  // Apply further cropping and scaling on the already cropped frame.
  auto cropped_frame2 = cropped_frame1->CropAndScale(
      kCroppedRect2.x(), kCroppedRect2.y(), kCroppedRect2.width(),
      kCroppedRect2.height(), kScaledSize2.width(), kScaledSize2.height());

  // Mapping produces a frame of the correct size.
  auto mapped_cropped_frame2 = cropped_frame2->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_cropped_frame2->width(), kScaledSize2.width());
  EXPECT_EQ(mapped_cropped_frame2->height(), kScaledSize2.height());
  // The mapping above should be backed by a frame that wraps |frame_360p|. We
  // can tell by looking at the coded size.
  adapted_frame = multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(
          // The second cropped rectangle is relative to the first one.
          gfx::Rect(kCroppedRect1.x() + kCroppedRect2.x(),
                    kCroppedRect1.y() + kCroppedRect2.y(),
                    kCroppedRect2.width(), kCroppedRect2.height()),
          kScaledSize2));
  ASSERT_TRUE(adapted_frame);
  EXPECT_EQ(adapted_frame->coded_size(), frame_360p->coded_size());
}

TEST(WebRtcVideoFrameAdapterTest, FrameFeedbackSetsRequireMappedFrame) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);
  const gfx::Size kSize360p(640, 360);

  scoped_refptr<WebRtcVideoFrameAdapter::SharedResources> resources =
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr);

  auto frame_720p = CreateTestFrame(kSize720p, kRect720p, kSize720p,
                                    media::VideoFrame::STORAGE_OWNED_MEMORY,
                                    media::VideoPixelFormat::PIXEL_FORMAT_NV12);

  // By default, the feedback is not set to require mapping.
  EXPECT_FALSE(resources->GetFeedback().require_mapped_frame);
  {
    // Do some scaling, but don't map it.
    rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
            frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
            resources));
    multi_buffer->Scale(kSize360p.width(), kSize360p.height());
  }
  EXPECT_FALSE(resources->GetFeedback().require_mapped_frame);
  {
    // Do map the buffer.
    rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
            frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
            resources));
    multi_buffer->Scale(kSize360p.width(), kSize360p.height())
        ->GetMappedFrameBuffer(kNv12);
  }
  EXPECT_TRUE(resources->GetFeedback().require_mapped_frame);
}

}  // namespace blink
