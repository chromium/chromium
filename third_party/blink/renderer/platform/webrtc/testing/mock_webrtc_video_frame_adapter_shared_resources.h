// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_TESTING_MOCK_WEBRTC_VIDEO_FRAME_ADAPTER_SHARED_RESOURCES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_TESTING_MOCK_WEBRTC_VIDEO_FRAME_ADAPTER_SHARED_RESOURCES_H_

#include "third_party/blink/renderer/platform/webrtc/webrtc_video_frame_adapter.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

class MockSharedResources : public WebRtcVideoFrameAdapter::SharedResources {
 public:
  MockSharedResources() : WebRtcVideoFrameAdapter::SharedResources(nullptr) {}

  MOCK_METHOD(scoped_refptr<media::VideoFrame>,
              CreateFrame,
              (media::VideoPixelFormat format,
               const gfx::Size& coded_size,
               const gfx::Rect& visible_rect,
               const gfx::Size& natural_size,
               base::TimeDelta timestamp));

  MOCK_METHOD(scoped_refptr<media::VideoFrame>,
              CreateTemporaryFrame,
              (media::VideoPixelFormat format,
               const gfx::Size& coded_size,
               const gfx::Rect& visible_rect,
               const gfx::Size& natural_size,
               base::TimeDelta timestamp));

  MOCK_METHOD(scoped_refptr<viz::RasterContextProvider>,
              GetRasterContextProvider,
              ());

  MOCK_METHOD(scoped_refptr<media::VideoFrame>,
              ConstructVideoFrameFromTexture,
              (scoped_refptr<media::VideoFrame> source_frame));

  MOCK_METHOD(scoped_refptr<media::VideoFrame>,
              ConstructVideoFrameFromGpu,
              (scoped_refptr<media::VideoFrame> source_frame));

  void ExpectCreateFrameWithRealImplementation() {
    EXPECT_CALL(*this, CreateFrame)
        .WillOnce(testing::Invoke(
            [this](media::VideoPixelFormat format, const gfx::Size& coded_size,
                   const gfx::Rect& visible_rect, const gfx::Size& natural_size,
                   base::TimeDelta timestamp) {
              return WebRtcVideoFrameAdapter::SharedResources::CreateFrame(
                  format, coded_size, visible_rect, natural_size, timestamp);
            }));
  }

  void ExpectCreateTemporaryFrameWithRealImplementation() {
    EXPECT_CALL(*this, CreateTemporaryFrame)
        .WillOnce(testing::Invoke([this](media::VideoPixelFormat format,
                                         const gfx::Size& coded_size,
                                         const gfx::Rect& visible_rect,
                                         const gfx::Size& natural_size,
                                         base::TimeDelta timestamp) {
          return WebRtcVideoFrameAdapter::SharedResources::CreateTemporaryFrame(
              format, coded_size, visible_rect, natural_size, timestamp);
        }));
  }

 private:
  friend class base::RefCountedThreadSafe<MockSharedResources>;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_TESTING_MOCK_WEBRTC_VIDEO_FRAME_ADAPTER_SHARED_RESOURCES_H_
