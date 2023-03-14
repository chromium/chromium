// Copyright 2020 The Chromium Authors
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

using ::testing::ElementsAre;
using ::testing::IsEmpty;

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

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

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
                                   format, base::TimeDelta());
          }));

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

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

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());
  auto frame_360p = CreateTestFrame(
      kSize360p, kRect360p, kSize360p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

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

TEST(WebRtcVideoFrameAdapterTest,
     MapPreScaledFrameEvenIfOriginalFrameHasSoftAppliedScaling) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);
  const gfx::Size kSize360p(640, 360);
  const gfx::Rect kRect360p(0, 0, 640, 360);

  // The strictness of the mock ensures no additional scaling.
  scoped_refptr<MockSharedResources> resources =
      new testing::StrictMock<MockSharedResources>();

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());
  auto frame_360p = CreateTestFrame(
      kSize360p, kRect360p, kSize360p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

  // Soft-apply scaling of |frame_720p| so that its natural size is 360p.
  // Because the soft-applied frame is still backed by a 720p coded size frame,
  // we should still prefer to map |frame_360p| when mapping at 360p size.
  scoped_refptr<media::VideoFrame> soft_scaled_frame =
      media::VideoFrame::WrapVideoFrame(frame_720p, frame_720p->format(),
                                        kRect720p, kSize360p);

  rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          soft_scaled_frame,
          std::vector<scoped_refptr<media::VideoFrame>>({frame_360p}),
          resources));

  // The adapter should reflect the natural size, not the coded size.
  EXPECT_EQ(multi_buffer->width(), kSize360p.width());
  EXPECT_EQ(multi_buffer->height(), kSize360p.height());

  // Mapping produces a frame of the correct size.
  auto mapped_frame = multi_buffer->GetMappedFrameBuffer(kNv12);
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
                                   format, base::TimeDelta());
          }));

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());
  auto frame_480p = CreateTestFrame(
      kSize480p, kRect480p, kSize480p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());
  auto frame_360p = CreateTestFrame(
      kSize360p, kRect360p, kSize360p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

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

// When pre-scaled frames are not available we should scale from previously
// scaled frames. E.g. scaling 720p to 480p and then to 360p should perform
// scales "720p -> 480p" and "480p -> 360p" (NOT "720p -> 360p").
TEST(WebRtcVideoFrameAdapterTest,
     MapScaledFrameScalesFromClosestPreviouslyScaledFrameWithoutCropping) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);
  const gfx::Size kSize480p(853, 480);
  const gfx::Size kSize360p(640, 360);

  scoped_refptr<MockSharedResources> resources =
      new testing::StrictMock<MockSharedResources>();
  EXPECT_CALL(*resources, CreateFrame)
      .WillOnce(testing::Invoke(
          [](media::VideoPixelFormat format, const gfx::Size& coded_size,
             const gfx::Rect& visible_rect, const gfx::Size& natural_size,
             base::TimeDelta timestamp) {
            return CreateTestFrame(coded_size, visible_rect, natural_size,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   format, base::TimeDelta());
          }));

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

  rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
          resources));

  // Hard-apply scaling to 480p. Because a pre-scaled 480p is not available, we
  // scale from 720p.
  auto scaled_frame_480p =
      multi_buffer->Scale(kSize480p.width(), kSize480p.height());
  auto mapped_frame_480p = scaled_frame_480p->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_frame_480p->width(), kSize480p.width());
  EXPECT_EQ(mapped_frame_480p->height(), kSize480p.height());
  // The 480p must have been scaled from a media::VideoFrame.
  EXPECT_TRUE(multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kRect720p, kSize480p)));
  // Hard-apply scaling to 360p. Because a pre-scaled 360p is not available, but
  // we did previously scale to 480p, the most efficient scale is 480p -> 360p.
  auto scaled_frame_360p =
      multi_buffer->Scale(kSize360p.width(), kSize360p.height());
  auto mapped_frame_360p = scaled_frame_360p->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_frame_360p->width(), kSize360p.width());
  EXPECT_EQ(mapped_frame_360p->height(), kSize360p.height());
  // The 360p should have gotten scaled from the previously mapped 480p frame,
  // so there should not be an associated media::VideoFrame here.
  EXPECT_FALSE(multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kRect720p, kSize360p)));
}

TEST(WebRtcVideoFrameAdapterTest,
     MapScaledFrameScalesFromClosestPreviouslyScaledFrameWithCropping) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kFullCodedSize720p(1280, 720);
  const gfx::Rect kFullVisibleRect(20, 20, 1240, 680);  // 20 pixel border.
  const gfx::Size kFullNaturalSize(620, 340);           // Scaled down by 2.

  scoped_refptr<MockSharedResources> resources =
      new testing::StrictMock<MockSharedResources>();
  EXPECT_CALL(*resources, CreateFrame)
      .WillOnce(testing::Invoke(
          [](media::VideoPixelFormat format, const gfx::Size& coded_size,
             const gfx::Rect& visible_rect, const gfx::Size& natural_size,
             base::TimeDelta timestamp) {
            return CreateTestFrame(coded_size, visible_rect, natural_size,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   format, base::TimeDelta());
          }));

  // Create a full frame with soft-applied cropping and scaling.
  auto full_frame = CreateTestFrame(
      kFullCodedSize720p, kFullVisibleRect, kFullNaturalSize,
      media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

  rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          full_frame, std::vector<scoped_refptr<media::VideoFrame>>(),
          resources));

  // Crop and scale some more and then map it.
  // Apply a 10 pixel border and downscale by a factor of 2 again.
  auto scaled_frame = multi_buffer->CropAndScale(10, 10, 600, 320, 300, 160);
  auto mapped_scaled_frame = scaled_frame->GetMappedFrameBuffer(kNv12);
  gfx::Size kScaledFrameSize(300, 160);
  EXPECT_EQ(mapped_scaled_frame->width(), kScaledFrameSize.width());
  EXPECT_EQ(mapped_scaled_frame->height(), kScaledFrameSize.height());
  // The cropping above is magnified due to scaling factors.
  gfx::Rect kScaledFrameVisibleRect(kFullVisibleRect.x() + (10 * 2),
                                    kFullVisibleRect.y() + (10 * 2), (600 * 2),
                                    (320 * 2));
  EXPECT_TRUE(multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kScaledFrameVisibleRect,
                                                kScaledFrameSize)));

  // Downscale by another factor of two.
  gfx::Size kTinyFrameSize(kScaledFrameSize.width() / 2,
                           kScaledFrameSize.height() / 2);
  auto tiny_frame =
      scaled_frame->Scale(kTinyFrameSize.width(), kTinyFrameSize.height());
  auto mapped_tiny_frame = tiny_frame->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_tiny_frame->width(), kTinyFrameSize.width());
  EXPECT_EQ(mapped_tiny_frame->height(), kTinyFrameSize.height());
  // Because we do not have any pre-scaled images, but we have mapped frames,
  // subsequent downscales should be based on the previous mappings rather than
  // the full frame.
  EXPECT_FALSE(multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kScaledFrameVisibleRect,
                                                kTinyFrameSize)));
}

TEST(WebRtcVideoFrameAdapterTest,
     MapScaledFrameDoesNotScaleFromPreviouslyScaledFrameWithOtherCrop) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);
  const gfx::Rect kCroppedRect(1272, 720);  // Crop only a few pixels.
  const gfx::Size kSize480p(853, 480);
  const gfx::Size kSize360p(640, 360);

  scoped_refptr<MockSharedResources> resources =
      new testing::StrictMock<MockSharedResources>();
  EXPECT_CALL(*resources, CreateFrame)
      .Times(2)
      .WillRepeatedly(testing::Invoke(
          [](media::VideoPixelFormat format, const gfx::Size& coded_size,
             const gfx::Rect& visible_rect, const gfx::Size& natural_size,
             base::TimeDelta timestamp) {
            return CreateTestFrame(coded_size, visible_rect, natural_size,
                                   media::VideoFrame::STORAGE_OWNED_MEMORY,
                                   format, base::TimeDelta());
          }));

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

  rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
          resources));

  // Hard-apply scaling to 480p WITH cropping.
  auto scaled_frame_480p = multi_buffer->CropAndScale(
      kCroppedRect.x(), kCroppedRect.y(), kCroppedRect.width(),
      kCroppedRect.height(), kSize480p.width(), kSize480p.height());
  auto mapped_frame_480p = scaled_frame_480p->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_frame_480p->width(), kSize480p.width());
  EXPECT_EQ(mapped_frame_480p->height(), kSize480p.height());
  // The 480p must have been scaled from a media::VideoFrame.
  EXPECT_TRUE(multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kCroppedRect, kSize480p)));
  // Hard-apply scaling to 360p WITHOUT cropping.
  auto scaled_frame_360p =
      multi_buffer->Scale(kSize360p.width(), kSize360p.height());
  auto mapped_frame_360p = scaled_frame_360p->GetMappedFrameBuffer(kNv12);
  EXPECT_EQ(mapped_frame_360p->width(), kSize360p.width());
  EXPECT_EQ(mapped_frame_360p->height(), kSize360p.height());
  // Because the previously mapped 480p buffer has cropping it cannot be used
  // for scaling, so 360p is produced from the 720p frame.
  EXPECT_TRUE(multi_buffer->GetAdaptedVideoBufferForTesting(
      WebRtcVideoFrameAdapter::ScaledBufferSize(kRect720p, kSize360p)));
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

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());
  auto frame_360p = CreateTestFrame(
      kSize360p, kRect360p, kSize360p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

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

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

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

TEST(WebRtcVideoFrameAdapterTest, FrameFeedbackSetsMappedSizesWithScale) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);
  const gfx::Size kSize360p(640, 360);

  scoped_refptr<WebRtcVideoFrameAdapter::SharedResources> resources =
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr);

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

  // By default, there is no mapping.
  EXPECT_THAT(resources->GetFeedback().mapped_sizes, IsEmpty());
  // Map the buffer at full scale, 720p.
  {
    rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
            frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
            resources));
    multi_buffer->GetMappedFrameBuffer(kNv12);
  }
  EXPECT_THAT(resources->GetFeedback().mapped_sizes, ElementsAre(kSize720p));
  // Each new adapter will override the old feedback value. This time, map the
  // buffer after scaling down to 360p. The previous mapping should be gone.
  {
    rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
            frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
            resources));
    multi_buffer->Scale(kSize360p.width(), kSize360p.height())
        ->GetMappedFrameBuffer(kNv12);
  }
  EXPECT_THAT(resources->GetFeedback().mapped_sizes, ElementsAre(kSize360p));
  // Do some scaling, but but never map it.
  {
    rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
            frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
            resources));
    multi_buffer->Scale(kSize360p.width(), kSize360p.height());
  }
  EXPECT_THAT(resources->GetFeedback().mapped_sizes, IsEmpty());
  // Map both 720p and 360p. The 360p mapping is implied by ToI420().
  {
    rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
            frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
            resources));
    multi_buffer->GetMappedFrameBuffer(kNv12);
    multi_buffer->Scale(kSize360p.width(), kSize360p.height())->ToI420();
  }
  EXPECT_THAT(resources->GetFeedback().mapped_sizes,
              ElementsAre(kSize720p, kSize360p));
}

TEST(WebRtcVideoFrameAdapterTest,
     FrameFeedbackSetsMappedSizesWithCropAndScale) {
  std::vector<webrtc::VideoFrameBuffer::Type> kNv12 = {
      webrtc::VideoFrameBuffer::Type::kNV12};
  const gfx::Size kSize720p(1280, 720);
  const gfx::Rect kRect720p(0, 0, 1280, 720);

  scoped_refptr<WebRtcVideoFrameAdapter::SharedResources> resources =
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr);

  auto frame_720p = CreateTestFrame(
      kSize720p, kRect720p, kSize720p, media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

  // Map a cropped section of the 720p buffer. Because the scale is 1:1, the
  // buffer that gets mapped should be 720p even though the natural size is of
  // a cropped area.
  const gfx::Rect kFullScaleCropRect(100, 100, kSize720p.width() - 200,
                                     kSize720p.height() - 200);
  {
    rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
            frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
            resources));
    multi_buffer
        ->CropAndScale(kFullScaleCropRect.x(), kFullScaleCropRect.y(),
                       kFullScaleCropRect.width(), kFullScaleCropRect.height(),
                       kFullScaleCropRect.width(), kFullScaleCropRect.height())
        ->GetMappedFrameBuffer(kNv12);
  }
  EXPECT_THAT(resources->GetFeedback().mapped_sizes, ElementsAre(kSize720p));
  // Scale down the cropped area by a factor of 2. The result will be a cropped
  // subsection of a 360p buffer, so 360p is what should get reported.
  const int kDownScaleFactor = 2;
  const gfx::Size kDownScaleBufferSize(kSize720p.width() / kDownScaleFactor,
                                       kSize720p.height() / kDownScaleFactor);
  const gfx::Size kDownScaleCropSize(
      kFullScaleCropRect.width() / kDownScaleFactor,
      kFullScaleCropRect.height() / kDownScaleFactor);
  {
    rtc::scoped_refptr<WebRtcVideoFrameAdapter> multi_buffer(
        new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
            frame_720p, std::vector<scoped_refptr<media::VideoFrame>>(),
            resources));
    multi_buffer
        ->CropAndScale(kFullScaleCropRect.x(), kFullScaleCropRect.y(),
                       kFullScaleCropRect.width(), kFullScaleCropRect.height(),
                       kDownScaleCropSize.width(), kDownScaleCropSize.height())
        ->GetMappedFrameBuffer(kNv12);
  }
  EXPECT_THAT(resources->GetFeedback().mapped_sizes,
              ElementsAre(kDownScaleBufferSize));
}

}  // namespace blink
