// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/legacy_webrtc_video_frame_adapter.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/blink/renderer/platform/webrtc/testing/mock_webrtc_video_frame_adapter_shared_resources.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "ui/gfx/gpu_memory_buffer.h"

using ::testing::_;
using ::testing::Return;

namespace blink {

class LegacyWebRtcVideoFrameAdapterParamTest
    : public ::testing::TestWithParam<
          std::tuple<media::VideoFrame::StorageType, media::VideoPixelFormat>> {
 public:
  LegacyWebRtcVideoFrameAdapterParamTest()
      : resources_(
            new LegacyWebRtcVideoFrameAdapter::SharedResources(nullptr)) {}

 protected:
  scoped_refptr<LegacyWebRtcVideoFrameAdapter::SharedResources> resources_;
};

namespace {
std::vector<LegacyWebRtcVideoFrameAdapterParamTest::ParamType> TestParams() {
  std::vector<LegacyWebRtcVideoFrameAdapterParamTest::ParamType> test_params;
  // All formats for owned memory.
  for (media::VideoPixelFormat format :
       LegacyWebRtcVideoFrameAdapter::AdaptableMappablePixelFormats()) {
    test_params.emplace_back(
        media::VideoFrame::StorageType::STORAGE_OWNED_MEMORY, format);
  }
  test_params.emplace_back(
      media::VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  return test_params;
}
}  // namespace

TEST_P(LegacyWebRtcVideoFrameAdapterParamTest, WidthAndHeight) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);

  media::VideoFrame::StorageType storage_type = std::get<0>(GetParam());
  media::VideoPixelFormat pixel_format = std::get<1>(GetParam());
  scoped_refptr<media::VideoFrame> frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize, storage_type, pixel_format);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter =
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(std::move(frame),
                                                               resources_);
  EXPECT_EQ(frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(frame_adapter->height(), kNaturalSize.height());
}

TEST_P(LegacyWebRtcVideoFrameAdapterParamTest, ToI420) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);

  media::VideoFrame::StorageType storage_type = std::get<0>(GetParam());
  media::VideoPixelFormat pixel_format = std::get<1>(GetParam());
  scoped_refptr<media::VideoFrame> frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize, storage_type, pixel_format);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter =
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(std::move(frame),
                                                               resources_);

  // The I420 frame should have the same size as the natural size.
  auto i420_frame = frame_adapter->ToI420();
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
}

INSTANTIATE_TEST_CASE_P(
    LegacyWebRtcVideoFrameAdapterParamTest,
    LegacyWebRtcVideoFrameAdapterParamTest,
    ::testing::ValuesIn(TestParams()),
    [](const auto& info) {
      return base::StrCat(
          {media::VideoFrame::StorageTypeToString(std::get<0>(info.param)), "_",
           media::VideoPixelFormatToString(std::get<1>(info.param))});
    });

TEST(LegacyWebRtcVideoFrameAdapterTest, ToI420DownScaleGmb) {
  base::test::ScopedFeatureList scoped_feautre_list;
  scoped_feautre_list.InitAndDisableFeature(
      blink::features::kWebRtcLibvpxEncodeNV12);
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<LegacyWebRtcVideoFrameAdapter::SharedResources> resources =
      new LegacyWebRtcVideoFrameAdapter::SharedResources(nullptr);
  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by GpuMemoryBuffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> gmb_frame_adapter(
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(
          std::move(gmb_frame), resources));
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

TEST(LegacyWebRtcVideoFrameAdapterTest, ToI420ADownScale) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<LegacyWebRtcVideoFrameAdapter::SharedResources> resources =
      new LegacyWebRtcVideoFrameAdapter::SharedResources(nullptr);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY,
                      media::VideoPixelFormat::PIXEL_FORMAT_I420A);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_adapter(
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), resources));
  EXPECT_EQ(owned_memory_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(owned_memory_frame_adapter->height(), kNaturalSize.height());

  // The I420A frame should have the same size as the natural size
  auto i420a_frame = owned_memory_frame_adapter->ToI420();
  ASSERT_TRUE(i420a_frame);
  EXPECT_EQ(webrtc::VideoFrameBuffer::Type::kI420A, i420a_frame->type());
  EXPECT_EQ(i420a_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420a_frame->height(), kNaturalSize.height());
}

TEST(LegacyWebRtcVideoFrameAdapterTest,
     Nv12WrapsGmbWhenNoScalingNeeededWithFeature) {
  base::test::ScopedFeatureList scoped_feautre_list;
  scoped_feautre_list.InitAndEnableFeature(
      blink::features::kWebRtcLibvpxEncodeNV12);
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  // Same size as visible rect so no scaling.
  const gfx::Size kNaturalSize = kVisibleRect.size();
  scoped_refptr<LegacyWebRtcVideoFrameAdapter::SharedResources> resources =
      new LegacyWebRtcVideoFrameAdapter::SharedResources(nullptr);

  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by GpuMemoryBuffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> gmb_frame_adapter(
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(gmb_frame,
                                                               resources));
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

TEST(LegacyWebRtcVideoFrameAdapterTest, Nv12ScalesGmbWithFeature) {
  base::test::ScopedFeatureList scoped_feautre_list;
  scoped_feautre_list.InitAndEnableFeature(
      blink::features::kWebRtcLibvpxEncodeNV12);
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<LegacyWebRtcVideoFrameAdapter::SharedResources> resources =
      new LegacyWebRtcVideoFrameAdapter::SharedResources(nullptr);

  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by GpuMemoryBuffer.
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> gmb_frame_adapter(
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(gmb_frame,
                                                               resources));
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

TEST(LegacyWebRtcVideoFrameAdapterTest, Nv12OwnedMemoryFrame) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize = kVisibleRect.size();
  scoped_refptr<LegacyWebRtcVideoFrameAdapter::SharedResources> resources =
      new LegacyWebRtcVideoFrameAdapter::SharedResources(nullptr);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY,
                      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_adapter(
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), resources));
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

// The default implementation of CropAndScale() is "ToI420() + scaling". This
// test verifies that CropAndScale() is overridden not to convert when scaling.
TEST(LegacyWebRtcVideoFrameAdapterTest, ScalingDoesNotConvertToI420) {
  const gfx::Size kFullSize(1280, 960);
  const gfx::Size kHalfSize(640, 480);
  scoped_refptr<LegacyWebRtcVideoFrameAdapter::SharedResources> resources =
      new LegacyWebRtcVideoFrameAdapter::SharedResources(nullptr);

  auto owned_memory_frame =
      CreateTestFrame(kFullSize, gfx::Rect(kFullSize), kFullSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY,
                      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> adapter(
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), resources));

  // Scale the frame.
  auto scaled_frame = adapter->Scale(kHalfSize.width(), kHalfSize.height());
  // It would also be correct to return a kNative frame that when mapped returns
  // kNV12 - this is what the modern adapter does. But the legacy adapter
  // implementation maps as part of scaling.
  EXPECT_EQ(scaled_frame->type(), webrtc::VideoFrameBuffer::Type::kNV12);
  EXPECT_EQ(scaled_frame->width(), kHalfSize.width());
  EXPECT_EQ(scaled_frame->height(), kHalfSize.height());
}

TEST(LegacyWebRtcVideoFrameAdapterTest, Nv12ScaleOwnedMemoryFrame) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<LegacyWebRtcVideoFrameAdapter::SharedResources> resources =
      new LegacyWebRtcVideoFrameAdapter::SharedResources(nullptr);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY,
                      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_adapter(
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), resources));
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

TEST(LegacyWebRtcVideoFrameAdapterTest,
     TextureFrameIsBlackWithNoSharedResources) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize, media::VideoFrame::STORAGE_OPAQUE,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), nullptr));
  EXPECT_EQ(frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(frame_adapter->height(), kNaturalSize.height());

  // The NV12 frame should have the same size as the natural size, but be black
  // since we can't handle the texture with no shared resources.
  auto i420_frame = frame_adapter->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
  EXPECT_EQ(0x0, i420_frame->DataY()[0]);
  EXPECT_EQ(0x80, i420_frame->DataU()[0]);
  EXPECT_EQ(0x80, i420_frame->DataV()[0]);
}

TEST(LegacyWebRtcVideoFrameAdapterTest,
     ConvertsTextureFrameWithSharedResources) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);

  scoped_refptr<MockLegacySharedResources> resources =
      base::MakeRefCounted<MockLegacySharedResources>();

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame = CreateTestFrame(
      kCodedSize, kVisibleRect, kNaturalSize, media::VideoFrame::STORAGE_OPAQUE,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> frame_adapter(
      new rtc::RefCountedObject<LegacyWebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), resources));
  EXPECT_EQ(frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(frame_adapter->height(), kNaturalSize.height());

  scoped_refptr<media::VideoFrame> memory_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY,
                      media::VideoPixelFormat::PIXEL_FORMAT_ARGB);
  // fill mock image with whilte color.
  memset(memory_frame->data(media::VideoFrame::kARGBPlane), 0xFF,
         kCodedSize.GetArea() * 4);

  // Should call texture conversion.
  resources->ExpectCreateFrameWithRealImplementation();
  resources->ExpectCreateTemporaryFrameWithRealImplementation();
  EXPECT_CALL(*resources, ConstructVideoFrameFromTexture(_))
      .WillOnce(Return(memory_frame));

  // The NV12 frame should have the same size as the natural size, but be black
  // since we can't handle the texture with no shared resources.
  auto i420_frame = frame_adapter->ToI420();
  ASSERT_TRUE(i420_frame);
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
  // Returned memory frame should not be replaced by a black frame.
  EXPECT_NE(0x0, i420_frame->DataY()[0]);
}
}  // namespace blink
