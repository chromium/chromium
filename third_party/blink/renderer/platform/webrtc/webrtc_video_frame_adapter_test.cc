// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace blink {

class WebRtcVideoFrameAdapterParamTest
    : public ::testing::TestWithParam<
          std::tuple<media::VideoFrame::StorageType, media::VideoPixelFormat>> {
 public:
  WebRtcVideoFrameAdapterParamTest()
      : pool_(new WebRtcVideoFrameAdapter::BufferPoolOwner()) {}

 protected:
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool_;
};

namespace {
std::vector<WebRtcVideoFrameAdapterParamTest::ParamType> TestParams() {
  std::vector<WebRtcVideoFrameAdapterParamTest::ParamType> test_params;
  // All formats for owned memory.
  for (media::VideoPixelFormat format :
       WebRtcVideoFrameAdapter::AdaptableMappablePixelFormats()) {
    test_params.emplace_back(
        media::VideoFrame::StorageType::STORAGE_OWNED_MEMORY, format);
  }
  test_params.emplace_back(
      media::VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  return test_params;
}
}  // namespace

TEST_P(WebRtcVideoFrameAdapterParamTest, WidthAndHeight) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);

  media::VideoFrame::StorageType storage_type = std::get<0>(GetParam());
  media::VideoPixelFormat pixel_format = std::get<1>(GetParam());
  scoped_refptr<media::VideoFrame> frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize, storage_type, pixel_format);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter =
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(std::move(frame),
                                                         pool_);
  EXPECT_EQ(frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(frame_adapter->height(), kNaturalSize.height());
}

TEST_P(WebRtcVideoFrameAdapterParamTest, ToI420) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);

  media::VideoFrame::StorageType storage_type = std::get<0>(GetParam());
  media::VideoPixelFormat pixel_format = std::get<1>(GetParam());
  scoped_refptr<media::VideoFrame> frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize, storage_type, pixel_format);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter =
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(std::move(frame),
                                                         pool_);

  // The I420 frame should have the same size as the natural size.
  auto i420_frame = frame_adapter->ToI420();
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
}

INSTANTIATE_TEST_CASE_P(
    WebRtcVideoFrameAdapterParamTest,
    WebRtcVideoFrameAdapterParamTest,
    ::testing::ValuesIn(TestParams()),
    [](const auto& info) {
      return base::StrCat(
          {media::VideoFrame::StorageTypeToString(std::get<0>(info.param)), "_",
           media::VideoPixelFormatToString(std::get<1>(info.param))});
    });

TEST(WebRtcVideoFrameAdapterTest, ToI420DownScaleGmb) {
  base::test::ScopedFeatureList scoped_feautre_list;
  scoped_feautre_list.InitAndDisableFeature(
      blink::features::kWebRtcLibvpxEncodeNV12);
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool =
      new WebRtcVideoFrameAdapter::BufferPoolOwner();
  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by GpuMemoryBuffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> gmb_frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(std::move(gmb_frame),
                                                         pool));
  EXPECT_EQ(gmb_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(gmb_frame_adapter->height(), kNaturalSize.height());

  // The I420 frame should have the same size as the natural size
  auto i420_frame = gmb_frame_adapter->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
  auto* get_i420_frame = gmb_frame_adapter->GetI420();
  ASSERT_TRUE(get_i420_frame);
  EXPECT_EQ(get_i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(get_i420_frame->height(), kNaturalSize.height());
}

TEST(WebRtcVideoFrameAdapterTest, ToI420ADownScale) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool =
      new WebRtcVideoFrameAdapter::BufferPoolOwner();

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY,
                      media::VideoPixelFormat::PIXEL_FORMAT_I420A);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), pool));
  EXPECT_EQ(owned_memory_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(owned_memory_frame_adapter->height(), kNaturalSize.height());

  // The I420A frame should have the same size as the natural size
  auto i420a_frame = owned_memory_frame_adapter->ToI420();
  ASSERT_TRUE(i420a_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kI420A, i420a_frame->type());
  EXPECT_EQ(i420a_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420a_frame->height(), kNaturalSize.height());
}

TEST(WebRtcVideoFrameAdapterTest, Nv12WrapsGmbWhenNoScalingNeeededWithFeature) {
  base::test::ScopedFeatureList scoped_feautre_list;
  scoped_feautre_list.InitAndEnableFeature(
      blink::features::kWebRtcLibvpxEncodeNV12);
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  // Same size as visible rect so no scaling.
  const gfx::Size kNaturalSize = kVisibleRect.size();
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool =
      new WebRtcVideoFrameAdapter::BufferPoolOwner();

  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by GpuMemoryBuffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> gmb_frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(gmb_frame, pool));
  EXPECT_EQ(gmb_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(gmb_frame_adapter->height(), kNaturalSize.height());

  // Under feature, expect that the adapted frame is NV12 with frame should
  // have the same size as the natural size.
  std::vector<webrtc::VideoFrameBuffer::Type> nv12_type{
      webrtc::VideoFrameBuffer::Type::kNV12};
  auto nv12_frame = gmb_frame_adapter->GetMappedFrameBuffer(nv12_type);
  ASSERT_TRUE(nv12_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kNV12, nv12_frame->type());
  EXPECT_EQ(nv12_frame->width(), kNaturalSize.width());
  EXPECT_EQ(nv12_frame->height(), kNaturalSize.height());

  // Even though we have an NV12 frame, ToI420 should return an I420 frame.
  std::vector<webrtc::VideoFrameBuffer::Type> i420_type{
      webrtc::VideoFrameBuffer::Type::kI420};
  EXPECT_FALSE(gmb_frame_adapter->GetMappedFrameBuffer(i420_type));
  auto i420_frame = gmb_frame_adapter->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
}

TEST(WebRtcVideoFrameAdapterTest, Nv12ScalesGmbWithFeature) {
  base::test::ScopedFeatureList scoped_feautre_list;
  scoped_feautre_list.InitAndEnableFeature(
      blink::features::kWebRtcLibvpxEncodeNV12);
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool =
      new WebRtcVideoFrameAdapter::BufferPoolOwner();

  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by GpuMemoryBuffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> gmb_frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(gmb_frame, pool));
  EXPECT_EQ(gmb_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(gmb_frame_adapter->height(), kNaturalSize.height());

  // Under feature, expect that the adapted frame is NV12 with frame should
  // have the same size as the natural size.
  std::vector<webrtc::VideoFrameBuffer::Type> nv12_type{
      webrtc::VideoFrameBuffer::Type::kNV12};
  auto nv12_frame = gmb_frame_adapter->GetMappedFrameBuffer(nv12_type);
  ASSERT_TRUE(nv12_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kNV12, nv12_frame->type());
  EXPECT_EQ(nv12_frame->width(), kNaturalSize.width());
  EXPECT_EQ(nv12_frame->height(), kNaturalSize.height());

  // Even though we have an NV12 frame, ToI420 should return an I420 frame.
  std::vector<webrtc::VideoFrameBuffer::Type> i420_type{
      webrtc::VideoFrameBuffer::Type::kI420};
  EXPECT_FALSE(gmb_frame_adapter->GetMappedFrameBuffer(i420_type));
  auto i420_frame = gmb_frame_adapter->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
}

TEST(WebRtcVideoFrameAdapterTest, Nv12OwnedMemoryFrame) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize = kVisibleRect.size();
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool =
      new WebRtcVideoFrameAdapter::BufferPoolOwner();

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY,
                      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), pool));
  EXPECT_EQ(owned_memory_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(owned_memory_frame_adapter->height(), kNaturalSize.height());

  // The NV12 frame should have the same size as the visible rect size
  std::vector<webrtc::VideoFrameBuffer::Type> nv12_type{
      webrtc::VideoFrameBuffer::Type::kNV12};
  auto nv12_frame = owned_memory_frame_adapter->GetMappedFrameBuffer(nv12_type);
  ASSERT_TRUE(nv12_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kNV12, nv12_frame->type());
  EXPECT_EQ(nv12_frame->width(), kVisibleRect.size().width());
  EXPECT_EQ(nv12_frame->height(), kVisibleRect.size().height());
}

TEST(WebRtcVideoFrameAdapterTest, Nv12ScaleOwnedMemoryFrame) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool =
      new WebRtcVideoFrameAdapter::BufferPoolOwner();

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY,
                      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), pool));
  EXPECT_EQ(owned_memory_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(owned_memory_frame_adapter->height(), kNaturalSize.height());

  // The NV12 frame should have the same size as the natural size.
  std::vector<webrtc::VideoFrameBuffer::Type> nv12_type{
      webrtc::VideoFrameBuffer::Type::kNV12};
  auto nv12_frame = owned_memory_frame_adapter->GetMappedFrameBuffer(nv12_type);
  ASSERT_TRUE(nv12_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kNV12, nv12_frame->type());
  EXPECT_EQ(nv12_frame->width(), kNaturalSize.width());
  EXPECT_EQ(nv12_frame->height(), kNaturalSize.height());
}

TEST(WebRtcVideoFrameAdapterTest, TextureFrameIsBlack) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool =
      new WebRtcVideoFrameAdapter::BufferPoolOwner();

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize, media::VideoFrame::STORAGE_OPAQUE,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), pool));
  EXPECT_EQ(frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(frame_adapter->height(), kNaturalSize.height());

  // The NV12 frame should have the same size as the natural size, but be black
  // since we can't handle the texture.
  auto i420_frame = frame_adapter->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
  EXPECT_EQ(0x0, i420_frame->DataY()[0]);
  EXPECT_EQ(0x80, i420_frame->DataU()[0]);
  EXPECT_EQ(0x80, i420_frame->DataV()[0]);
}
}  // namespace blink
