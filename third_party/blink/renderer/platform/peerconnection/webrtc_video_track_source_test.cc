// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_video_track_source.h"

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "media/base/format_utils.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/video_frame_utils.h"
#include "third_party/blink/renderer/platform/webrtc/convert_to_webrtc_video_frame_buffer.h"
#include "third_party/webrtc/api/video/video_frame.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::Sequence;

namespace blink {

void ExpectUpdateRectEquals(const gfx::Rect& expected,
                            const webrtc::VideoFrame::UpdateRect actual) {
  EXPECT_EQ(expected.x(), actual.offset_x);
  EXPECT_EQ(expected.y(), actual.offset_y);
  EXPECT_EQ(expected.width(), actual.width);
  EXPECT_EQ(expected.height(), actual.height);
}

class MockVideoSink : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
  MOCK_METHOD1(OnFrame, void(const webrtc::VideoFrame&));
};

TEST(WebRtcVideoTrackSourceRefreshFrameTest, CallsRefreshFrame) {
  bool called = false;
  scoped_refptr<WebRtcVideoTrackSource> track_source =
      new rtc::RefCountedObject<WebRtcVideoTrackSource>(
          /*is_screencast=*/false,
          /*needs_denoising=*/std::nullopt,
          base::BindLambdaForTesting([](const media::VideoCaptureFeedback&) {}),
          base::BindLambdaForTesting([&called] { called = true; }),
          /*gpu_factories=*/nullptr);
  track_source->RequestRefreshFrame();
  EXPECT_TRUE(called);
}

class WebRtcVideoTrackSourceTest
    : public ::testing::TestWithParam<
          std::tuple<media::VideoFrame::StorageType, media::VideoPixelFormat>>,
      public media::FakeGpuMemoryBuffer::MapCallbackController {
 public:
  WebRtcVideoTrackSourceTest()
      : shared_resources_(
            base::MakeRefCounted<WebRtcVideoFrameAdapter::SharedResources>(
                /*gpu_factories=*/nullptr)),
        track_source_(new rtc::RefCountedObject<WebRtcVideoTrackSource>(
            /*is_screencast=*/false,
            /*needs_denoising=*/std::nullopt,
            base::BindRepeating(&WebRtcVideoTrackSourceTest::ProcessFeedback,
                                base::Unretained(this)),
            base::BindLambdaForTesting([] {}),
            /*gpu_factories=*/nullptr,
            shared_resources_)) {
    track_source_->AddOrUpdateSink(&mock_sink_, rtc::VideoSinkWants());
  }

  void ProcessFeedback(const media::VideoCaptureFeedback& feedback) {
    feedback_ = feedback;
  }

  ~WebRtcVideoTrackSourceTest() override {
    if (track_source_) {
      track_source_->RemoveSink(&mock_sink_);
    }
  }

  struct FrameParameters {
    const gfx::Size coded_size;
    gfx::Rect visible_rect;
    const gfx::Size natural_size;
    media::VideoFrame::StorageType storage_type;
    media::VideoPixelFormat pixel_format;
  };

  void RegisterCallback(base::OnceCallback<void(bool)> result_cb) override {
    map_callbacks_.push_back(std::move(result_cb));
  }

  void InvokeNextMapCallback() {
    ASSERT_FALSE(map_callbacks_.empty());
    auto cb = std::move(map_callbacks_.front());
    map_callbacks_.pop_front();
    std::move(cb).Run(true);
  }

  void SendTestFrame(const FrameParameters& frame_parameters,
                     base::TimeDelta timestamp) {
    scoped_refptr<media::VideoFrame> frame = CreateTestFrame(
        frame_parameters.coded_size, frame_parameters.visible_rect,
        frame_parameters.natural_size, frame_parameters.storage_type,
        frame_parameters.pixel_format, timestamp);
    track_source_->OnFrameCaptured(frame);
  }

  void SendTestFrameWithMappableGMB(const FrameParameters& frame_parameters,
                                    base::TimeDelta timestamp,
                                    bool premapped) {
    std::unique_ptr<media::FakeGpuMemoryBuffer> fake_gmb =
        std::make_unique<media::FakeGpuMemoryBuffer>(
            frame_parameters.coded_size,
            media::VideoPixelFormatToGfxBufferFormat(
                frame_parameters.pixel_format)
                .value(),
            premapped, this);
    scoped_refptr<media::VideoFrame> frame = CreateTestFrame(
        frame_parameters.coded_size, frame_parameters.visible_rect,
        frame_parameters.natural_size, frame_parameters.storage_type,
        frame_parameters.pixel_format, timestamp, std::move(fake_gmb));
    track_source_->OnFrameCaptured(frame);
  }

  void SendTestFrameAndVerifyFeedback(const FrameParameters& frame_parameters,
                                      int max_pixels,
                                      float max_framerate) {
    scoped_refptr<media::VideoFrame> frame = CreateTestFrame(
        frame_parameters.coded_size, frame_parameters.visible_rect,
        frame_parameters.natural_size, frame_parameters.storage_type,
        frame_parameters.pixel_format, base::TimeDelta());
    track_source_->OnFrameCaptured(frame);
    EXPECT_EQ(feedback_.max_pixels, max_pixels);
    EXPECT_EQ(feedback_.max_framerate_fps, max_framerate);
  }

  void SendTestFrameWithUpdateRect(const FrameParameters& frame_parameters,
                                   int capture_counter,
                                   const gfx::Rect& update_rect) {
    scoped_refptr<media::VideoFrame> frame = CreateTestFrame(
        frame_parameters.coded_size, frame_parameters.visible_rect,
        frame_parameters.natural_size, frame_parameters.storage_type,
        frame_parameters.pixel_format, base::TimeDelta());
    frame->metadata().capture_counter = capture_counter;
    frame->metadata().capture_update_rect = update_rect;
    track_source_->OnFrameCaptured(frame);
  }

  void SendTestFrameWithColorSpace(const FrameParameters& frame_parameters,
                                   const gfx::ColorSpace& color_space) {
    scoped_refptr<media::VideoFrame> frame = CreateTestFrame(
        frame_parameters.coded_size, frame_parameters.visible_rect,
        frame_parameters.natural_size, frame_parameters.storage_type,
        frame_parameters.pixel_format, base::TimeDelta());
    frame->set_color_space(color_space);
    track_source_->OnFrameCaptured(frame);
  }

  WebRtcVideoTrackSource::FrameAdaptationParams FrameAdaptation_KeepAsIs(
      const gfx::Size& natural_size) {
    return WebRtcVideoTrackSource::FrameAdaptationParams{
        false /*should_drop_frame*/,
        0 /*crop_x*/,
        0 /*crop_y*/,
        natural_size.width() /*crop_width*/,
        natural_size.height() /*crop_height*/,
        natural_size.width() /*scale_to_width*/,
        natural_size.height() /*scale_to_height*/
    };
  }

  WebRtcVideoTrackSource::FrameAdaptationParams FrameAdaptation_DropFrame() {
    return WebRtcVideoTrackSource::FrameAdaptationParams{
        true /*should_drop_frame*/,
        0 /*crop_x*/,
        0 /*crop_y*/,
        0 /*crop_width*/,
        0 /*crop_height*/,
        0 /*scale_to_width*/,
        0 /*scale_to_height*/
    };
  }

  WebRtcVideoTrackSource::FrameAdaptationParams FrameAdaptation_Scale(
      const gfx::Size& natural_size,
      const gfx::Size& scale_to_size) {
    return WebRtcVideoTrackSource::FrameAdaptationParams{
        false /*should_drop_frame*/,
        0 /*crop_x*/,
        0 /*crop_y*/,
        natural_size.width() /*crop_width*/,
        natural_size.height() /*crop_height*/,
        scale_to_size.width() /*scale_to_width*/,
        scale_to_size.height() /*scale_to_height*/
    };
  }

  void SetRequireMappedFrame(bool require_mapped_frame) {
    shared_resources_->SetFeedback(
        media::VideoCaptureFeedback().RequireMapped(require_mapped_frame));
  }

 protected:
  MockVideoSink mock_sink_;
  scoped_refptr<WebRtcVideoFrameAdapter::SharedResources> shared_resources_;
  scoped_refptr<WebRtcVideoTrackSource> track_source_;
  media::VideoCaptureFeedback feedback_;
  WTF::Deque<base::OnceCallback<void(bool)>> map_callbacks_;
};

namespace {
std::vector<WebRtcVideoTrackSourceTest::ParamType> TestParams() {
  std::vector<WebRtcVideoTrackSourceTest::ParamType> test_params;
  // All formats for owned memory.
  for (media::VideoPixelFormat format :
       GetPixelFormatsMappableToWebRtcVideoFrameBuffer()) {
    test_params.emplace_back(
        media::VideoFrame::StorageType::STORAGE_OWNED_MEMORY, format);
  }
  test_params.emplace_back(
      media::VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER,
      media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  test_params.emplace_back(media::VideoFrame::STORAGE_OPAQUE,
                           media::VideoPixelFormat::PIXEL_FORMAT_NV12);
  return test_params;
}
}  // namespace

// Tests that the two generated test frames are received in sequence and have
// correct |capture_time_identifier| set in webrtc::VideoFrame.
TEST_P(WebRtcVideoTrackSourceTest, TestTimestamps) {
  FrameParameters frame_parameters = {
      .coded_size = gfx::Size(640, 480),
      .visible_rect = gfx::Rect(0, 60, 640, 360),
      .natural_size = gfx::Size(640, 360),
      .storage_type = std::get<0>(GetParam()),
      .pixel_format = std::get<1>(GetParam())};

  Sequence s;
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .InSequence(s)
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ASSERT_TRUE(frame.capture_time_identifier().has_value());
        EXPECT_EQ(frame.capture_time_identifier().value().us(), 0);
      }));
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .InSequence(s)
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ASSERT_TRUE(frame.capture_time_identifier().has_value());
        EXPECT_EQ(frame.capture_time_identifier().value().us(), 16666);
      }));
  SendTestFrame(frame_parameters, base::Seconds(0));
  const float kFps = 60.0;
  SendTestFrame(frame_parameters, base::Seconds(1 / kFps));
}

TEST_P(WebRtcVideoTrackSourceTest, CropFrameTo640360) {
  const gfx::Size kNaturalSize(640, 360);
  FrameParameters frame_parameters = {
      .coded_size = gfx::Size(640, 480),
      .visible_rect = gfx::Rect(0, 60, 640, 360),
      .natural_size = kNaturalSize,
      .storage_type = std::get<0>(GetParam()),
      .pixel_format = std::get<1>(GetParam())};

  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(kNaturalSize));

  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kNaturalSize](const webrtc::VideoFrame& frame) {
        EXPECT_EQ(kNaturalSize.width(), frame.width());
        EXPECT_EQ(kNaturalSize.height(), frame.height());
      }));
  SendTestFrame(frame_parameters, base::TimeDelta());
}

TEST_P(WebRtcVideoTrackSourceTest, TestColorSpaceSettings) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /* enabled_features*/ {media::kWebRTCColorAccuracy},
      /* disabled_features*/ {});
  FrameParameters frame_parameters = {
      .coded_size = gfx::Size(640, 480),
      .visible_rect = gfx::Rect(0, 60, 640, 360),
      .natural_size = gfx::Size(640, 360),
      .storage_type = std::get<0>(GetParam()),
      .pixel_format = std::get<1>(GetParam())};

  Sequence s;

  EXPECT_CALL(mock_sink_, OnFrame(_))
      .InSequence(s)
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ASSERT_TRUE(frame.color_space().has_value());
        EXPECT_EQ(frame.color_space().value().matrix(),
                  webrtc::ColorSpace::MatrixID::kSMPTE170M);
        EXPECT_EQ(frame.color_space().value().transfer(),
                  webrtc::ColorSpace::TransferID::kBT709);
        EXPECT_EQ(frame.color_space().value().primaries(),
                  webrtc::ColorSpace::PrimaryID::kBT709);
        EXPECT_EQ(frame.color_space().value().range(),
                  webrtc::ColorSpace::RangeID::kLimited);
      }));
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .InSequence(s)
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ASSERT_TRUE(frame.color_space().has_value());
        EXPECT_EQ(frame.color_space().value().matrix(),
                  webrtc::ColorSpace::MatrixID::kBT709);
        EXPECT_EQ(frame.color_space().value().transfer(),
                  webrtc::ColorSpace::TransferID::kBT709);
        EXPECT_EQ(frame.color_space().value().primaries(),
                  webrtc::ColorSpace::PrimaryID::kBT709);
        EXPECT_EQ(frame.color_space().value().range(),
                  webrtc::ColorSpace::RangeID::kFull);
      }));

  // For default REC709{BT709,BT709,BT709,Limited}, we will not set color space
  // and transmit it by RTP since decoder side would guess it if color space is
  // invalid.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .InSequence(s)
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ASSERT_FALSE(frame.color_space().has_value());
      }));

  gfx::ColorSpace color_range_limited(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::BT709,
      gfx::ColorSpace::MatrixID::SMPTE170M, gfx::ColorSpace::RangeID::LIMITED);
  SendTestFrameWithColorSpace(frame_parameters, color_range_limited);

  gfx::ColorSpace color_range_full(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::BT709,
      gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::FULL);
  SendTestFrameWithColorSpace(frame_parameters, color_range_full);

  gfx::ColorSpace default_bt709_color_space(
      gfx::ColorSpace::PrimaryID::BT709, gfx::ColorSpace::TransferID::BT709,
      gfx::ColorSpace::MatrixID::BT709, gfx::ColorSpace::RangeID::LIMITED);
  SendTestFrameWithColorSpace(frame_parameters, default_bt709_color_space);
}

TEST_P(WebRtcVideoTrackSourceTest, SetsFeedback) {
  FrameParameters frame_parameters = {
      .coded_size = gfx::Size(640, 480),
      .visible_rect = gfx::Rect(0, 60, 640, 360),
      .natural_size = gfx::Size(640, 360),
      .storage_type = std::get<0>(GetParam()),
      .pixel_format = std::get<1>(GetParam())};
  const gfx::Size kScaleToSize = gfx::Size(320, 180);
  const float k5Fps = 5.0;

  rtc::VideoSinkWants sink_wants;
  sink_wants.max_pixel_count = kScaleToSize.GetArea();
  sink_wants.max_framerate_fps = static_cast<int>(k5Fps);
  track_source_->SetSinkWantsForTesting(sink_wants);

  EXPECT_CALL(mock_sink_, OnFrame(_));
  SendTestFrameAndVerifyFeedback(frame_parameters, kScaleToSize.GetArea(),
                                 k5Fps);
}

TEST_P(WebRtcVideoTrackSourceTest, CropFrameTo320320) {
  const gfx::Size kNaturalSize(320, 320);
  FrameParameters frame_parameters = {
      .coded_size = gfx::Size(640, 480),
      .visible_rect = gfx::Rect(80, 0, 480, 480),
      .natural_size = kNaturalSize,
      .storage_type = std::get<0>(GetParam()),
      .pixel_format = std::get<1>(GetParam())};

  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(kNaturalSize));

  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kNaturalSize](const webrtc::VideoFrame& frame) {
        EXPECT_EQ(kNaturalSize.width(), frame.width());
        EXPECT_EQ(kNaturalSize.height(), frame.height());
      }));
  SendTestFrame(frame_parameters, base::TimeDelta());
}

TEST_P(WebRtcVideoTrackSourceTest, Scale720To640360) {
  const gfx::Size kNaturalSize(640, 360);
  FrameParameters frame_parameters = {
      .coded_size = gfx::Size(1280, 720),
      .visible_rect = gfx::Rect(0, 0, 1280, 720),
      .natural_size = kNaturalSize,
      .storage_type = std::get<0>(GetParam()),
      .pixel_format = std::get<1>(GetParam())};
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(kNaturalSize));

  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kNaturalSize](const webrtc::VideoFrame& frame) {
        EXPECT_EQ(kNaturalSize.width(), frame.width());
        EXPECT_EQ(kNaturalSize.height(), frame.height());
      }));
  SendTestFrame(frame_parameters, base::TimeDelta());
}

TEST_P(WebRtcVideoTrackSourceTest, UpdateRectWithNoTransform) {
  const gfx::Rect kVisibleRect(0, 0, 640, 480);
  FrameParameters frame_parameters = {.coded_size = gfx::Size(640, 480),
                                      .visible_rect = kVisibleRect,
                                      .natural_size = gfx::Size(640, 480),
                                      .storage_type = std::get<0>(GetParam()),
                                      .pixel_format = std::get<1>(GetParam())};
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(frame_parameters.natural_size));

  // Any UPDATE_RECT for the first received frame is expected to get
  // ignored and the full frame should be marked as updated.
  const gfx::Rect kUpdateRect1(1, 2, 3, 4);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(0, 0, frame.width(), frame.height()),
                               frame.update_rect());
      }));
  int capture_counter = 101;  // arbitrary absolute value
  SendTestFrameWithUpdateRect(frame_parameters, capture_counter, kUpdateRect1);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Update rect for second frame should get passed along.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kUpdateRect1](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(kUpdateRect1, frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect1);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Simulate the next frame getting dropped
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_DropFrame());
  const gfx::Rect kUpdateRect2(2, 3, 4, 5);
  EXPECT_CALL(mock_sink_, OnFrame(_)).Times(0);
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect2);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // The |update_rect| for the next frame is expected to contain the union
  // of the current an previous |update_rects|.
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(frame_parameters.natural_size));
  const gfx::Rect kUpdateRect3(3, 4, 5, 6);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(
          Invoke([kUpdateRect2, kUpdateRect3](const webrtc::VideoFrame& frame) {
            gfx::Rect expected_update_rect(kUpdateRect2);
            expected_update_rect.Union(kUpdateRect3);
            ExpectUpdateRectEquals(expected_update_rect, frame.update_rect());
          }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect3);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Simulate a gap in |capture_counter|. This is expected to cause the whole
  // frame to get marked as updated.
  ++capture_counter;
  const gfx::Rect kUpdateRect4(4, 5, 6, 7);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kVisibleRect](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(kVisibleRect, frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect4);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Important edge case (expected to be fairly common): An empty update rect
  // indicates that nothing has changed.
  const gfx::Rect kEmptyRectWithZeroOrigin(0, 0, 0, 0);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_TRUE(frame.update_rect().IsEmpty());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kEmptyRectWithZeroOrigin);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  const gfx::Rect kEmptyRectWithNonZeroOrigin(10, 20, 0, 0);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_TRUE(frame.update_rect().IsEmpty());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kEmptyRectWithNonZeroOrigin);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // A frame without a CAPTURE_COUNTER and CAPTURE_UPDATE_RECT is treated as the
  // whole content having changed.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kVisibleRect](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(kVisibleRect, frame.update_rect());
      }));
  SendTestFrame(frame_parameters, base::TimeDelta());
  Mock::VerifyAndClearExpectations(&mock_sink_);
}

TEST_P(WebRtcVideoTrackSourceTest, UpdateRectWithCropFromUpstream) {
  const gfx::Rect kVisibleRect(100, 50, 200, 80);
  FrameParameters frame_parameters = {.coded_size = gfx::Size(640, 480),
                                      .visible_rect = kVisibleRect,
                                      .natural_size = gfx::Size(200, 80),
                                      .storage_type = std::get<0>(GetParam()),
                                      .pixel_format = std::get<1>(GetParam())};
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_KeepAsIs(frame_parameters.natural_size));

  // Any UPDATE_RECT for the first received frame is expected to get
  // ignored and the full frame should be marked as updated.
  const gfx::Rect kUpdateRect1(120, 70, 160, 40);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(0, 0, frame.width(), frame.height()),
                               frame.update_rect());
      }));
  int capture_counter = 101;  // arbitrary absolute value
  SendTestFrameWithUpdateRect(frame_parameters, capture_counter, kUpdateRect1);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Update rect for second frame should get passed along.
  // Update rect fully contained in crop region.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(
          Invoke([kUpdateRect1, kVisibleRect](const webrtc::VideoFrame& frame) {
            gfx::Rect expected_update_rect(kUpdateRect1);
            expected_update_rect.Offset(-kVisibleRect.x(), -kVisibleRect.y());
            ExpectUpdateRectEquals(expected_update_rect, frame.update_rect());
          }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect1);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Update rect outside crop region.
  const gfx::Rect kUpdateRect2(2, 3, 4, 5);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_TRUE(frame.update_rect().IsEmpty());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect2);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // Update rect partly overlapping crop region.
  const gfx::Rect kUpdateRect3(kVisibleRect.x() + 10, kVisibleRect.y() + 8,
                               kVisibleRect.width(), kVisibleRect.height());
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([kVisibleRect](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(10, 8, kVisibleRect.width() - 10,
                                         kVisibleRect.height() - 8),
                               frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect3);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // When crop origin changes, the whole frame is expected to be marked as
  // changed.
  const gfx::Rect kVisibleRect2(kVisibleRect.x() + 1, kVisibleRect.y(),
                                kVisibleRect.width(), kVisibleRect.height());
  frame_parameters.visible_rect = kVisibleRect2;
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(0, 0, frame.width(), frame.height()),
                               frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect1);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // When crop size changes, the whole frame is expected to be marked as
  // changed.
  const gfx::Rect kVisibleRect3(kVisibleRect2.x(), kVisibleRect2.y(),
                                kVisibleRect2.width(),
                                kVisibleRect2.height() - 1);
  frame_parameters.visible_rect = kVisibleRect3;
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(0, 0, frame.width(), frame.height()),
                               frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect1);
  Mock::VerifyAndClearExpectations(&mock_sink_);
}

TEST_P(WebRtcVideoTrackSourceTest, UpdateRectWithScaling) {
  const gfx::Size kNaturalSize = gfx::Size(200, 80);
  FrameParameters frame_parameters = {
      .coded_size = gfx::Size(640, 480),
      .visible_rect = gfx::Rect(100, 50, 200, 80),
      .natural_size = kNaturalSize,
      .storage_type = std::get<0>(GetParam()),
      .pixel_format = std::get<1>(GetParam())};
  const gfx::Size kScaleToSize = gfx::Size(120, 50);
  if (frame_parameters.storage_type == media::VideoFrame::STORAGE_OPAQUE) {
    // Texture has no cropping support yet http://crbug/503653.
    return;
  }
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_Scale(kNaturalSize, kScaleToSize));

  // Any UPDATE_RECT for the first received frame is expected to get
  // ignored and no update rect should be set.
  const gfx::Rect kUpdateRect1(120, 70, 160, 40);
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_FALSE(frame.has_update_rect());
      }));
  int capture_counter = 101;  // arbitrary absolute value
  SendTestFrameWithUpdateRect(frame_parameters, capture_counter, kUpdateRect1);
  Mock::VerifyAndClearExpectations(&mock_sink_);

  // When scaling is applied and UPDATE_RECT is not empty, we scale the
  // update rect.
  // Calculated by hand according to KNaturalSize and KScaleToSize.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        ExpectUpdateRectEquals(gfx::Rect(10, 10, 100, 30), frame.update_rect());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter,
                              kUpdateRect1);

  // When UPDATE_RECT is empty, we expect to deliver an empty UpdateRect even if
  // scaling is applied.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_TRUE(frame.update_rect().IsEmpty());
      }));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter, gfx::Rect());

  // When UPDATE_RECT is empty, but the scaling has changed, we expect to
  // deliver no known update_rect.
  EXPECT_CALL(mock_sink_, OnFrame(_))
      .WillOnce(Invoke([](const webrtc::VideoFrame& frame) {
        EXPECT_FALSE(frame.has_update_rect());
      }));
  const gfx::Size kScaleToSize2 = gfx::Size(60, 26);
  track_source_->SetCustomFrameAdaptationParamsForTesting(
      FrameAdaptation_Scale(kNaturalSize, kScaleToSize2));
  SendTestFrameWithUpdateRect(frame_parameters, ++capture_counter, gfx::Rect());

  Mock::VerifyAndClearExpectations(&mock_sink_);
}

TEST_P(WebRtcVideoTrackSourceTest, PassesMappedFramesInOrder) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FrameParameters frame_parameters = {
      .coded_size = gfx::Size(640, 480),
      .visible_rect = gfx::Rect(0, 60, 640, 360),
      .natural_size = gfx::Size(640, 360),
      .storage_type = std::get<0>(GetParam()),
      .pixel_format = std::get<1>(GetParam())};
  if (frame_parameters.storage_type !=
      media::VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    // Mapping is only valid for GMB backed frames.
    return;
  }
  constexpr int kSentFrames = 10;
  Sequence s;
  for (int i = 0; i < kSentFrames; ++i) {
    EXPECT_CALL(mock_sink_, OnFrame(_))
        .InSequence(s)
        .WillOnce(Invoke([=](const webrtc::VideoFrame& frame) {
          EXPECT_EQ(frame.capture_time_identifier().value().us(), 1000000 * i);
        }));
  }

  SetRequireMappedFrame(false);
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(0),
                               /*premapped=*/false);

  SetRequireMappedFrame(true);
  // This will be the 1st async frame.
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(1),
                               /*premapped=*/false);

  SetRequireMappedFrame(true);
  // This will be the 2nd async frame.
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(2),
                               /*premapped=*/false);

  SetRequireMappedFrame(true);
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(3),
                               /*premapped=*/true);

  // This will return the 1st async frame.
  InvokeNextMapCallback();

  SetRequireMappedFrame(true);
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(4),
                               /*premapped=*/true);

  SetRequireMappedFrame(true);
  // This will be the 3rd async frame.
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(5),
                               /*premapped=*/false);

  SetRequireMappedFrame(false);
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(6),
                               /*premapped=*/false);

  // This will return the 2nd async frame.
  InvokeNextMapCallback();

  SetRequireMappedFrame(true);
  // This will be the 4th async frame.
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(7),
                               /*premapped=*/false);

  // This will return the 3rd async frame.
  InvokeNextMapCallback();

  // This will return the 4th async frame.
  InvokeNextMapCallback();

  SetRequireMappedFrame(true);
  // This will be the 5th async frame.
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(8),
                               /*premapped=*/false);

  SetRequireMappedFrame(true);
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(9),
                               /*premapped=*/true);

  // This will return the 5th async frame.
  InvokeNextMapCallback();
}

TEST_P(WebRtcVideoTrackSourceTest, DoesntCrashOnLateCallbacks) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FrameParameters frame_parameters = {
      .coded_size = gfx::Size(640, 480),
      .visible_rect = gfx::Rect(0, 60, 640, 360),
      .natural_size = gfx::Size(640, 360),
      .storage_type = std::get<0>(GetParam()),
      .pixel_format = std::get<1>(GetParam())};
  if (frame_parameters.storage_type !=
      media::VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER) {
    // Mapping is only valid for GMB backed frames.
    return;
  }

  SetRequireMappedFrame(true);
  SendTestFrameWithMappableGMB(frame_parameters, base::Seconds(0),
                               /*premapped=*/false);

  track_source_->Dispose();
  track_source_->RemoveSink(&mock_sink_);
  track_source_.reset();

  InvokeNextMapCallback();
}

INSTANTIATE_TEST_SUITE_P(
    WebRtcVideoTrackSourceTest,
    WebRtcVideoTrackSourceTest,
    testing::ValuesIn(TestParams()),
    [](const auto& info) {
      return base::StrCat(
          {media::VideoFrame::StorageTypeToString(std::get<0>(info.param)), "_",
           media::VideoPixelFormatToString(std::get<1>(info.param))});
    });

}  // namespace blink
