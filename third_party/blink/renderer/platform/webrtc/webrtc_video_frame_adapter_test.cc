// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"

#include "base/test/scoped_feature_list.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/webrtc/api/video/video_frame_buffer.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace blink {

TEST(WebRtcVideoFrameAdapterTest, WidthAndHeight) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool =
      new WebRtcVideoFrameAdapter::BufferPoolOwner();

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), pool));
  EXPECT_EQ(owned_memory_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(owned_memory_frame_adapter->height(), kNaturalSize.height());

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by GpuMemoryBuffer.
  auto gmb_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_GPU_MEMORY_BUFFER);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> gmb_frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(std::move(gmb_frame),
                                                         pool));
  EXPECT_EQ(gmb_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(gmb_frame_adapter->height(), kNaturalSize.height());
}

TEST(WebRtcVideoFrameAdapterTest, ToI420DownScale) {
  const gfx::Size kCodedSize(1280, 960);
  const gfx::Rect kVisibleRect(0, 120, 1280, 720);
  const gfx::Size kNaturalSize(640, 360);
  scoped_refptr<WebRtcVideoFrameAdapter::BufferPoolOwner> pool =
      new WebRtcVideoFrameAdapter::BufferPoolOwner();

  // The adapter should report width and height from the natural size for
  // VideoFrame backed by owned memory.
  auto owned_memory_frame =
      CreateTestFrame(kCodedSize, kVisibleRect, kNaturalSize,
                      media::VideoFrame::STORAGE_OWNED_MEMORY);
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> owned_memory_frame_adapter(
      new rtc::RefCountedObject<WebRtcVideoFrameAdapter>(
          std::move(owned_memory_frame), pool));
  EXPECT_EQ(owned_memory_frame_adapter->width(), kNaturalSize.width());
  EXPECT_EQ(owned_memory_frame_adapter->height(), kNaturalSize.height());

  // The I420 frame should have the same size as the natural size
  auto i420_frame = owned_memory_frame_adapter->ToI420();
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
}

TEST(WebRtcVideoFrameAdapterTest, ToI420DownScaleGmb) {
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
  EXPECT_EQ(i420_frame->width(), kNaturalSize.width());
  EXPECT_EQ(i420_frame->height(), kNaturalSize.height());
}

TEST(WebRtcVideoFrameAdapterTest, Nv12WrapsGmbWhenNoScalingNeeeded) {
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

}  // namespace blink
