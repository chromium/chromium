// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/blink/renderer/platform/webrtc/testing/mock_webrtc_video_frame_adapter_shared_resources.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "ui/gfx/gpu_memory_buffer.h"

using ::testing::_;
using ::testing::Return;

namespace blink {

class ConvertToWebRtcVideoFrameBufferParamTest
    : public ::testing::TestWithParam<
          std::tuple<media::VideoFrame::StorageType, media::VideoPixelFormat>> {
 protected:
  scoped_refptr<WebRtcVideoFrameAdapter::SharedResources> resources_ =
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr);
};

namespace {
std::vector<ConvertToWebRtcVideoFrameBufferParamTest::ParamType> TestParams() {
  std::vector<ConvertToWebRtcVideoFrameBufferParamTest::ParamType> test_params;
  // All formats for owned memory.
  for (media::VideoPixelFormat format :
       GetPixelFormatsMappableToWebRtcVideoFrameBuffer()) {
    test_params.emplace_back(
        media::VideoFrame::StorageType::STORAGE_OWNED_MEMORY, format);
  }
  test_params.emplace_back(
      media::VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  return test_params;
}
}  // namespace

TEST_P(ConvertToWebRtcVideoFrameBufferParamTest, ToI420) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);

  media::VideoFrame::StorageType storage_type = std::get<0>(GetParam());
  media::VideoPixelFormat pixel_format = std::get<1>(GetParam());
  scoped_refptr<media::VideoFrame> frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize, storage_type,
                      pixel_format, base::TimeDelta());
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
      ConvertToWebRtcVideoFrameBuffer(std::move(frame), resources_);

  // The I420 frame should have the same size as the natural size.
  auto i420_frame = frame_buffer->ToI420();
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
}

INSTANTIATE_TEST_SUITE_P(
    ConvertToWebRtcVideoFrameBufferParamTest,
    ConvertToWebRtcVideoFrameBufferParamTest,
    ::testing::ValuesIn(TestParams()),
    [](const auto& info) {
      return base::StrCat(
          {media::VideoFrame::StorageTypeToString(std::get<0>(info.param)), "_",
           media::VideoPixelFormatToString(std::get<1>(info.param))});
    });

TEST(ConvertToWebRtcVideoFrameBufferTest, ToI420ADownScale) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  auto resources =
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize,
      media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_I420A, base::TimeDelta());
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_buffer =
      ConvertToWebRtcVideoFrameBuffer(std::move(owned_memory_frame), resources);
  EXPECT_EQ(owned_memory_frame_buffer->width(), kNaturalSize.width());
  EXPECT_EQ(owned_memory_frame_buffer->height(), kNaturalSize.height());

  // The I420A frame should have the same size as the natural size
  auto i420a_frame = owned_memory_frame_buffer->ToI420();
  ASSERT_TRUE(i420a_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kI420A, i420a_frame->type());
  EXPECT_EQ(i420a_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420a_frame->height(), kNaturalSize.height());
}

TEST(ConvertToWebRtcVideoFrameBufferTest,
     Nv12WrapsGmbWhenNoScalingNeeededWithFeature) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  // Same size as visible rect so no scaling.
  const gfx::Size kNaturalSize = kVisibleRect.size();
  auto resources =
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr);

  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by GpuMemoryBuffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> gmb_frame_buffer =
      ConvertToWebRtcVideoFrameBuffer(std::move(gmb_frame), resources);
  EXPECT_EQ(gmb_frame_buffer->width(), kNaturalSize.width());
  EXPECT_EQ(gmb_frame_buffer->height(), kNaturalSize.height());

  // Under feature, expect that the adapted frame is NV12 with frame should
  // have the same size as the natural size.
  auto* nv12_frame = gmb_frame_buffer->GetNV12();
  ASSERT_TRUE(nv12_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kNV12, nv12_frame->type());
  EXPECT_EQ(nv12_frame->width(), kNaturalSize.width());
  EXPECT_EQ(nv12_frame->height(), kNaturalSize.height());

  // Even though we have an NV12 frame, ToI420 should return an I420 frame.
  auto i420_frame = gmb_frame_buffer->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
}

TEST(ConvertToWebRtcVideoFrameBufferTest, Nv12ScalesGmbWithFeature) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  auto resources =
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr);

  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by GpuMemoryBuffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> gmb_frame_buffer =
      ConvertToWebRtcVideoFrameBuffer(gmb_frame, resources);
  EXPECT_EQ(gmb_frame_buffer->width(), kNaturalSize.width());
  EXPECT_EQ(gmb_frame_buffer->height(), kNaturalSize.height());

  // Under feature, expect that the adapted frame is NV12 with frame should
  // have the same size as the natural size.
  auto* nv12_frame = gmb_frame_buffer->GetNV12();
  ASSERT_TRUE(nv12_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kNV12, nv12_frame->type());
  EXPECT_EQ(nv12_frame->width(), kNaturalSize.width());
  EXPECT_EQ(nv12_frame->height(), kNaturalSize.height());

  // Even though we have an NV12 frame, ToI420 should return an I420 frame.
  auto i420_frame = gmb_frame_buffer->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
}

TEST(ConvertToWebRtcVideoFrameBufferTest, Nv12OwnedMemoryFrame) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize = kVisibleRect.size();
  auto resources =
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize,
      media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_buffer =
      ConvertToWebRtcVideoFrameBuffer(std::move(owned_memory_frame), resources);
  EXPECT_EQ(owned_memory_frame_buffer->width(), kNaturalSize.width());
  EXPECT_EQ(owned_memory_frame_buffer->height(), kNaturalSize.height());

  // The NV12 frame should have the same size as the visible rect size
  auto* nv12_frame = owned_memory_frame_buffer->GetNV12();
  ASSERT_TRUE(nv12_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kNV12, nv12_frame->type());
  EXPECT_EQ(nv12_frame->width(), kVisibleRect.size().width());
  EXPECT_EQ(nv12_frame->height(), kVisibleRect.size().height());
}

TEST(ConvertToWebRtcVideoFrameBufferTest, Nv12ScaleOwnedMemoryFrame) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  auto resources =
      base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(nullptr);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize,
      media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_buffer =
      ConvertToWebRtcVideoFrameBuffer(std::move(owned_memory_frame), resources);
  EXPECT_EQ(owned_memory_frame_buffer->width(), kNaturalSize.width());
  EXPECT_EQ(owned_memory_frame_buffer->height(), kNaturalSize.height());

  // The NV12 frame should have the same size as the natural size.
  auto* nv12_frame = owned_memory_frame_buffer->GetNV12();
  ASSERT_TRUE(nv12_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kNV12, nv12_frame->type());
  EXPECT_EQ(nv12_frame->width(), kNaturalSize.width());
  EXPECT_EQ(nv12_frame->height(), kNaturalSize.height());
}

TEST(ConvertToWebRtcVideoFrameBufferTest,
     TextureFrameIsBlackWithNoSharedResources) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize, media::VideoFrame::STORAGE_OPAQUE,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
      ConvertToWebRtcVideoFrameBuffer(std::move(owned_memory_frame), nullptr);
  EXPECT_EQ(frame_buffer->width(), kNaturalSize.width());
  EXPECT_EQ(frame_buffer->height(), kNaturalSize.height());

  // The NV12 frame should have the same size as the natural size, but be black
  // since we can't handle the texture with no shared resources.
  auto i420_frame = frame_buffer->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
  EXPECT_EQ(0x0, i420_frame->DataY()[0]);
  EXPECT_EQ(0x80, i420_frame->DataU()[0]);
  EXPECT_EQ(0x80, i420_frame->DataV()[0]);
}

TEST(ConvertToWebRtcVideoFrameBufferTest,
     ConvertsTextureFrameWithSharedResources) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);

  scoped_refptr<MockSharedResources> resources =
      base::MakeRefCounted<MockSharedResources>();

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize, media::VideoFrame::STORAGE_OPAQUE,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12, base::TimeDelta());

  scoped_refptr<media::VideoFrame> memory_frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize,
      media::VideoFrame::STORAGE_OWNED_MEMORY,
      media::VideoPixelFormat::PIXEL_FORMAT_ARGB, base::TimeDelta());
  // fill mock image with whilte color.
  memset(memory_frame->writable_data(media::VideoFrame::Plane::kARGB), 0xFF,
         kCodedSize.GetArea() * 4);

  // Should call texture conversion.
  resources->ExpectCreateFrameWithRealImplementation();
  resources->ExpectConvertAndScaleWithRealImplementation();
  EXPECT_CALL(*resources, ConstructVideoFrameFromTexture(_))
      .WillOnce(Return(memory_frame));

  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_buffer =
      ConvertToWebRtcVideoFrameBuffer(std::move(owned_memory_frame), resources);
  EXPECT_EQ(frame_buffer->width(), kNaturalSize.width());
  EXPECT_EQ(frame_buffer->height(), kNaturalSize.height());

  // The NV12 frame should have the same size as the natural size, but be black
  // since we can't handle the texture with no shared resources.
  auto i420_frame = frame_buffer->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
  // Returned memory frame should not be replaced by a black frame.
  EXPECT_NE(0x0, i420_frame->DataY()[0]);
}
}  // namespace blink
