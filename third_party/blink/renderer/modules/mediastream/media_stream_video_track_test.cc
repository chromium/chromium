// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"

#include <stdint.h>

#include <array>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_settings.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/mock_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/modules/peerconnection/media_stream_video_webrtc_sink.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// To avoid symbol collisions in jumbo builds.
namespace media_stream_video_track_test {

using base::test::RunOnceClosure;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Values;

using ContentHintType = WebMediaStreamTrack::ContentHintType;
using VideoFrameStats = MediaStreamTrackPlatform::VideoFrameStats;

const uint8_t kBlackValue = 0x00;
const uint8_t kColorValue = 0xAB;
const int kMockSourceWidth = 640;
const int kMockSourceHeight = 480;
const double kMinFrameRate = 30.0;

class MockEmitLogMessageCb {
 public:
  MOCK_METHOD1(EmitLogMessage, void(const std::string&));

  base::RepeatingCallback<void(const std::string&)> Callback() {
    return base::BindRepeating(base::BindLambdaForTesting(
        [this](const std::string& message) { EmitLogMessage(message); }));
  }
};

class MockEventListener : public NativeEventListener {
 public:
  MOCK_METHOD(void, Invoke, (ExecutionContext*, Event*));
};

class MediaStreamVideoTrackTest
    : public testing::TestWithParam<ContentHintType> {
 public:
  MediaStreamVideoTrackTest() : mock_source_(nullptr) {}

  ~MediaStreamVideoTrackTest() override {}

  void TearDown() override {
    mock_source_ = nullptr;
    source_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  void DeliverVideoFrameAndWaitForRenderer(
      scoped_refptr<media::VideoFrame> frame,
      MockMediaStreamVideoSink* sink) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*sink, OnVideoFrame)
        .WillOnce(RunOnceClosure(std::move(quit_closure)));
    mock_source()->DeliverVideoFrame(std::move(frame));
    run_loop.Run();
  }

  void DeliverEncodedVideoFrameAndWait(scoped_refptr<EncodedVideoFrame> frame,
                                       MockMediaStreamVideoSink* sink) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*sink, OnEncodedVideoFrame).WillOnce([&](base::TimeTicks) {
      std::move(quit_closure).Run();
    });
    mock_source()->DeliverEncodedVideoFrame(frame);
    run_loop.Run();
  }

  void DeliverDefaultSizeVideoFrameAndWaitForRenderer(
      MockMediaStreamVideoSink* sink) {
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateColorFrame(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight),
            kColorValue, kColorValue, kColorValue, base::TimeDelta());
    DeliverVideoFrameAndWaitForRenderer(frame, sink);
  }

 protected:
  virtual void InitializeSource() {
    source_ = nullptr;
    auto mock_source = std::make_unique<MockMediaStreamVideoSource>(
        media::VideoCaptureFormat(
            gfx::Size(kMockSourceWidth, kMockSourceHeight), 30.0,
            media::PIXEL_FORMAT_I420),
        false);
    mock_source_ = mock_source.get();
    MediaStreamDevice device = mock_source_->device();
    device.type = mojom::blink::MediaStreamType::DEVICE_VIDEO_CAPTURE;
    mock_source_->SetDevice(device);
    source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
        false /* remote */, std::move(mock_source));
  }

  void InitializeDisplayCaptureSource() {
    InitializeSource();
    MediaStreamDevice device = mock_source_->device();
    device.type = mojom::blink::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
    device.display_media_info = media::mojom::DisplayMediaInformation::New(
        media::mojom::DisplayCaptureSurfaceType::BROWSER,
        /*logical_surface=*/true, media::mojom::CursorCaptureType::NEVER,
        /*capture_handle=*/nullptr,
        /*zoom_level=*/100);
    mock_source_->SetDevice(device);
  }

  // Create a track that's associated with |mock_source_|.
  WebMediaStreamTrack CreateTrack() {
    const bool enabled = true;
    WebMediaStreamTrack track = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        enabled);
    if (!source_started_) {
      mock_source_->StartMockedSource();
      source_started_ = true;
    }
    return track;
  }

  // Create a track that's associated with |mock_source_| and has the given
  // |adapter_settings|.
  WebMediaStreamTrack CreateTrackWithSettings(
      const VideoTrackAdapterSettings& adapter_settings) {
    const bool enabled = true;
    WebMediaStreamTrack track = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, adapter_settings, std::optional<bool>(), false, 0.0,
        nullptr, false, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        enabled);
    if (!source_started_) {
      mock_source_->StartMockedSource();
      source_started_ = true;
    }
    return track;
  }

  void UpdateVideoSourceToRespondToRequestRefreshFrame() {
    source_ = nullptr;
    auto mock_source = std::make_unique<MockMediaStreamVideoSource>(
        media::VideoCaptureFormat(
            gfx::Size(kMockSourceWidth, kMockSourceHeight), 30.0,
            media::PIXEL_FORMAT_I420),
        true);
    mock_source_ = mock_source.get();
    source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
        false /* remote */, std::move(mock_source));
  }

  void DepleteIOCallbacks() {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    mock_source()->video_task_runner()->PostTask(
        FROM_HERE,
        base::BindLambdaForTesting([&] { std::move(quit_closure).Run(); }));
    run_loop.Run();
  }

  MockMediaStreamVideoSource* mock_source() { return mock_source_; }
  MediaStreamSource* stream_source() const { return source_; }

 private:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  Persistent<MediaStreamSource> source_;
  // |mock_source_| is owned by |source_|.
  raw_ptr<MockMediaStreamVideoSource> mock_source_;
  bool source_started_ = false;
};

TEST_F(MediaStreamVideoTrackTest, AddAndRemoveSink) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);

  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());

  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);

  sink.DisconnectFromTrack();

  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::CreateBlackFrame(
      gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                MediaStreamVideoSource::kDefaultHeight));
  mock_source()->DeliverVideoFrame(frame);
  // Wait for the video task runner to complete delivering frames.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, sink.number_of_frames());
}

class CheckThreadHelper {
 public:
  CheckThreadHelper(base::OnceClosure callback, bool* correct)
      : callback_(std::move(callback)), correct_(correct) {}

  ~CheckThreadHelper() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    *correct_ = true;
    std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
  raw_ptr<bool> correct_;
  THREAD_CHECKER(thread_checker_);
};

void CheckThreadVideoFrameReceiver(
    CheckThreadHelper* helper,
    scoped_refptr<media::VideoFrame> frame,
    base::TimeTicks estimated_capture_time) {
  // Do nothing.
}

// Checks that the callback given to the track is reset on the right thread.
TEST_F(MediaStreamVideoTrackTest, ResetCallbackOnThread) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();

  base::RunLoop run_loop;
  bool correct = false;
  sink.ConnectToTrackWithCallback(
      track, blink::BindRepeating(&CheckThreadVideoFrameReceiver,
                                  base::Owned(new CheckThreadHelper(
                                      run_loop.QuitClosure(), &correct))));
  sink.DisconnectFromTrack();
  run_loop.Run();
  EXPECT_TRUE(correct) << "Not called on correct thread.";
}

TEST_F(MediaStreamVideoTrackTest, SetEnabled) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);

  MediaStreamVideoTrack* video_track = MediaStreamVideoTrack::From(track);

  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());
  EXPECT_EQ(kColorValue,
            *sink.last_frame()->data(media::VideoFrame::Plane::kY));

  video_track->SetEnabled(false);
  EXPECT_FALSE(sink.enabled());

  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(2, sink.number_of_frames());
  EXPECT_EQ(kBlackValue,
            *sink.last_frame()->data(media::VideoFrame::Plane::kY));

  video_track->SetEnabled(true);
  EXPECT_TRUE(sink.enabled());
  DeliverDefaultSizeVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(3, sink.number_of_frames());
  EXPECT_EQ(kColorValue,
            *sink.last_frame()->data(media::VideoFrame::Plane::kY));
  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, SourceDetached) {
  InitializeSource();
  WebMediaStreamTrack track = CreateTrack();
  MockMediaStreamVideoSink sink;
  auto* video_track = MediaStreamVideoTrack::From(track);
  video_track->StopAndNotify(base::DoNothing());
  sink.ConnectToTrack(track);
  sink.ConnectEncodedToTrack(track);
  video_track->SetEnabled(true);
  video_track->SetEnabled(false);
  MediaStreamTrackPlatform::Settings settings;
  video_track->GetSettings(settings);
  sink.DisconnectFromTrack();
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, SourceStopped) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive, sink.state());

  mock_source()->StopSource();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded, sink.state());
  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, StopLastTrack) {
  InitializeSource();
  MockMediaStreamVideoSink sink1;
  WebMediaStreamTrack track1 = CreateTrack();
  sink1.ConnectToTrack(track1);
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive, sink1.state());

  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source()->GetReadyState());

  MockMediaStreamVideoSink sink2;
  WebMediaStreamTrack track2 = CreateTrack();
  sink2.ConnectToTrack(track2);
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive, sink2.state());

  MediaStreamVideoTrack* const native_track1 =
      MediaStreamVideoTrack::From(track1);
  native_track1->Stop();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded, sink1.state());
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source()->GetReadyState());
  sink1.DisconnectFromTrack();

  MediaStreamVideoTrack* const native_track2 =
      MediaStreamVideoTrack::From(track2);
  native_track2->Stop();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded, sink2.state());
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source()->GetReadyState());
  sink2.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, CheckTrackRequestsFrame) {
  InitializeSource();
  UpdateVideoSourceToRespondToRequestRefreshFrame();
  WebMediaStreamTrack track = CreateTrack();

  // Add sink and expect to get a frame.
  MockMediaStreamVideoSink sink;
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnVideoFrame)
      .WillOnce(RunOnceClosure(std::move(quit_closure)));
  sink.ConnectToTrack(track);
  run_loop.Run();
  EXPECT_EQ(1, sink.number_of_frames());

  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, GetSettings) {
  InitializeSource();
  WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::From(track);
  MediaStreamTrackPlatform::Settings settings;
  native_track->GetSettings(settings);
  // These values come straight from the mock video track implementation.
  EXPECT_EQ(640, settings.width);
  EXPECT_EQ(480, settings.height);
  EXPECT_EQ(30.0, settings.frame_rate);
  EXPECT_EQ(MediaStreamTrackPlatform::FacingMode::kNone, settings.facing_mode);
}

TEST_F(MediaStreamVideoTrackTest, GetSettingsWithAdjustment) {
  InitializeSource();
  const int kAdjustedWidth = 600;
  const int kAdjustedHeight = 400;
  const double kAdjustedFrameRate = 20.0;
  VideoTrackAdapterSettings adapter_settings(
      gfx::Size(kAdjustedWidth, kAdjustedHeight), 0.0, 10000.0,
      kAdjustedFrameRate);
  WebMediaStreamTrack track = CreateTrackWithSettings(adapter_settings);
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::From(track);
  MediaStreamTrackPlatform::Settings settings;
  native_track->GetSettings(settings);
  EXPECT_EQ(kAdjustedWidth, settings.width);
  EXPECT_EQ(kAdjustedHeight, settings.height);
  EXPECT_EQ(kAdjustedFrameRate, settings.frame_rate);
  EXPECT_EQ(MediaStreamTrackPlatform::FacingMode::kNone, settings.facing_mode);
}

TEST_F(MediaStreamVideoTrackTest, GetSettingsStopped) {
  InitializeSource();
  WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::From(track);
  native_track->Stop();
  MediaStreamTrackPlatform::Settings settings;
  native_track->GetSettings(settings);
  EXPECT_EQ(-1, settings.width);
  EXPECT_EQ(-1, settings.height);
  EXPECT_EQ(-1, settings.frame_rate);
  EXPECT_EQ(MediaStreamTrackPlatform::FacingMode::kNone, settings.facing_mode);
  EXPECT_TRUE(settings.device_id.IsNull());
}

TEST_F(MediaStreamVideoTrackTest, DeliverFramesAndGetSettings) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::From(track);
  EXPECT_FALSE(native_track->max_frame_rate().has_value());
  MediaStreamTrackPlatform::Settings settings;

  auto frame1 = media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400));
  DeliverVideoFrameAndWaitForRenderer(std::move(frame1), &sink);
  native_track->GetSettings(settings);
  EXPECT_EQ(600, settings.width);
  EXPECT_EQ(400, settings.height);
  EXPECT_EQ(std::nullopt, settings.physical_frame_size);

  auto frame2 = media::VideoFrame::CreateBlackFrame(gfx::Size(200, 300));

  media::VideoFrameMetadata metadata = frame2->metadata();
  metadata.source_size = gfx::Size(600, 400);
  frame2->set_metadata(metadata);
  DeliverVideoFrameAndWaitForRenderer(std::move(frame2), &sink);
  native_track->GetSettings(settings);
  EXPECT_EQ(200, settings.width);
  EXPECT_EQ(300, settings.height);
  ASSERT_NE(settings.physical_frame_size, std::nullopt);
  EXPECT_EQ(600, settings.physical_frame_size->width());
  EXPECT_EQ(400, settings.physical_frame_size->height());

  sink.DisconnectFromTrack();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
TEST_F(MediaStreamVideoTrackTest, ScreenPixelRatioDoesNotIncludePageZoom) {
  V8TestingScope v8_scope;
  InitializeDisplayCaptureSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  Persistent<MediaStreamComponent> media_stream_component = *track;
  MediaStreamTrackImpl* mst = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), media_stream_component);

  auto frame1 = media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400));
  media::VideoFrameMetadata metadata = frame1->metadata();
  metadata.source_size = gfx::Size(600, 400);
  metadata.device_scale_factor = 2.0f;
  frame1->set_metadata(metadata);
  DeliverVideoFrameAndWaitForRenderer(frame1, &sink);
  MediaTrackSettings* settings = mst->getSettings();
  ASSERT_TRUE(settings->hasScreenPixelRatio());
  EXPECT_EQ(mst->GetZoomLevelForTesting(), 100);
  EXPECT_EQ(settings->screenPixelRatio(), 2.0f);

  // Changing zoom size should not change pixel ratio
  media_stream_component->Source()->OnZoomLevelChange(*mst->device(), 150);
  DeliverVideoFrameAndWaitForRenderer(frame1, &sink);
  settings = mst->getSettings();
  ASSERT_TRUE(settings->hasScreenPixelRatio());
  EXPECT_EQ(mst->GetZoomLevelForTesting(), 150);
  EXPECT_EQ(settings->screenPixelRatio(), 2.0f);

  sink.DisconnectFromTrack();
}
#endif

TEST_F(MediaStreamVideoTrackTest,
       ChangingFrameMetadataTriggersConfigurationChangeEvent) {
  V8TestingScope v8_scope;
  InitializeDisplayCaptureSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::From(track);
  Persistent<MediaStreamComponent> media_stream_component = *track;
  MediaStreamTrackPlatform::Settings settings;
  MediaStreamTrackImpl* mst = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), media_stream_component);
  testing::StrictMock<MockEventListener>* event_listener =
      MakeGarbageCollected<testing::StrictMock<MockEventListener>>();
  mst->addEventListener(event_type_names::kConfigurationchange, event_listener);

  EXPECT_CALL(*event_listener, Invoke(_, _)).Times(2);

  // Changing device_scale_factor triggers an event.
  auto frame1 = media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400));
  media::VideoFrameMetadata metadata = frame1->metadata();
  metadata.device_scale_factor = 2.0f;
  frame1->set_metadata(metadata);
  DeliverVideoFrameAndWaitForRenderer(std::move(frame1), &sink);
  native_track->GetSettings(settings);
  EXPECT_EQ(std::nullopt, settings.physical_frame_size);
  EXPECT_EQ(2.0f, settings.device_scale_factor);

  // Changing metadata source_size triggers an event.
  auto frame2 = media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400));
  metadata.source_size = gfx::Size(600, 400);
  frame2->set_metadata(metadata);
  DeliverVideoFrameAndWaitForRenderer(std::move(frame2), &sink);
  native_track->GetSettings(settings);
  ASSERT_NE(std::nullopt, settings.physical_frame_size);
  EXPECT_EQ(600, settings.physical_frame_size->width());
  EXPECT_EQ(400, settings.physical_frame_size->height());
  EXPECT_EQ(2.0f, settings.device_scale_factor);

  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, FrameStatsIncrementsForEnabledTracks) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::From(track);
  EXPECT_FALSE(native_track->max_frame_rate().has_value());

  // Initially, no fames have been delivered.
  VideoFrameStats stats = native_track->GetVideoFrameStats();
  EXPECT_EQ(stats.deliverable_frames, 0u);
  EXPECT_EQ(stats.discarded_frames, 0u);
  EXPECT_EQ(stats.dropped_frames, 0u);

  // Deliver a frame an expect counter to increment to 1.
  DeliverVideoFrameAndWaitForRenderer(
      media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400)), &sink);
  stats = native_track->GetVideoFrameStats();
  EXPECT_EQ(stats.deliverable_frames, 1u);
  EXPECT_EQ(stats.discarded_frames, 0u);
  EXPECT_EQ(stats.dropped_frames, 0u);

  // Discard one frame (due to frame rate decimation) and drop two frames (other
  // reasons);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kGpuMemoryBufferMapFailed);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kGpuMemoryBufferMapFailed);
  DepleteIOCallbacks();
  stats = native_track->GetVideoFrameStats();
  EXPECT_EQ(stats.deliverable_frames, 1u);
  EXPECT_EQ(stats.discarded_frames, 1u);
  EXPECT_EQ(stats.dropped_frames, 2u);

  // And some more...
  DeliverVideoFrameAndWaitForRenderer(
      media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400)), &sink);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested);
  DepleteIOCallbacks();
  stats = native_track->GetVideoFrameStats();
  EXPECT_EQ(stats.deliverable_frames, 2u);
  EXPECT_EQ(stats.discarded_frames, 3u);
  EXPECT_EQ(stats.dropped_frames, 2u);

  // Disable the track and verify the frame counters do NOT increment, even as
  // frame delivery and dropped callbacks are invoked.
  native_track->SetEnabled(false);
  DeliverVideoFrameAndWaitForRenderer(
      media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400)), &sink);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kGpuMemoryBufferMapFailed);
  DepleteIOCallbacks();
  stats = native_track->GetVideoFrameStats();
  EXPECT_EQ(stats.deliverable_frames, 2u);
  EXPECT_EQ(stats.discarded_frames, 3u);
  EXPECT_EQ(stats.dropped_frames, 2u);

  // Enable it again, and business as usual...
  native_track->SetEnabled(true);
  DeliverVideoFrameAndWaitForRenderer(
      media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400)), &sink);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::
          kResolutionAdapterFrameRateIsHigherThanRequested);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kGpuMemoryBufferMapFailed);
  DepleteIOCallbacks();
  stats = native_track->GetVideoFrameStats();
  EXPECT_EQ(stats.deliverable_frames, 3u);
  EXPECT_EQ(stats.discarded_frames, 4u);
  EXPECT_EQ(stats.dropped_frames, 3u);

  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, DroppedFramesGetLoggedInUMA) {
  base::HistogramTester histogram_tester;

  InitializeSource();
  CreateTrack();
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded);
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  DepleteIOCallbacks();

  histogram_tester.ExpectBucketCount(
      "Media.VideoCapture.Track.FrameDrop.DeviceCapture",
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat,
      2);
  histogram_tester.ExpectBucketCount(
      "Media.VideoCapture.Track.FrameDrop.DeviceCapture",
      media::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded, 1);
}

// Tests that too many frames dropped for the same reason emits a special UMA
// log and disables further logging
TEST_F(MediaStreamVideoTrackTest,
       DroppedFrameLoggingGetsDisabledIfTooManyConsecutiveDropsForSameReason) {
  base::HistogramTester histogram_tester;

  InitializeSource();
  CreateTrack();
  for (int i = 0;
       i < MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount;
       i++) {
    mock_source()->DropFrame(
        media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  }
  DepleteIOCallbacks();
  histogram_tester.ExpectBucketCount(
      "Media.VideoCapture.Track.FrameDrop.DeviceCapture",
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat,
      MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount);

  // Add one more count after already having reached the max allowed.
  // This should not get counted.
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  DepleteIOCallbacks();
  histogram_tester.ExpectBucketCount(
      "Media.VideoCapture.Track.FrameDrop.DeviceCapture",
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat,
      MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount);
}

TEST_F(MediaStreamVideoTrackTest,
       DeliveredFrameInBetweenDroppedFramesResetsCounter) {
  base::HistogramTester histogram_tester;

  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  for (int i = 0;
       i <
       MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount - 1;
       i++) {
    mock_source()->DropFrame(
        media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  }
  DeliverVideoFrameAndWaitForRenderer(
      media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400)), &sink);

  for (int i = 0;
       i < MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount;
       i++) {
    mock_source()->DropFrame(
        media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  }
  DepleteIOCallbacks();
  histogram_tester.ExpectBucketCount(
      "Media.VideoCapture.Track.FrameDrop.DeviceCapture",
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat,
      2 * MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount -
          1);
}

TEST_F(MediaStreamVideoTrackTest, DeliveredFrameReenablesDroppedFrameLogging) {
  base::HistogramTester histogram_tester;

  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  // Drop enough frames to disable logging
  for (int i = 0;
       i <
       MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount + 1;
       i++) {
    mock_source()->DropFrame(
        media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  }
  DeliverVideoFrameAndWaitForRenderer(
      media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400)), &sink);

  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  DepleteIOCallbacks();
  histogram_tester.ExpectBucketCount(
      "Media.VideoCapture.Track.FrameDrop.DeviceCapture",
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat,
      MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount + 1);
}

TEST_F(MediaStreamVideoTrackTest,
       ChangeInDropReasonReenablesDroppedFrameLogging) {
  base::HistogramTester histogram_tester;

  InitializeSource();
  CreateTrack();
  // Drop enough frames to disable logging
  for (int i = 0;
       i <
       MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount + 1;
       i++) {
    mock_source()->DropFrame(
        media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  }

  // Drop for a different reason
  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded);

  mock_source()->DropFrame(
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat);
  DepleteIOCallbacks();
  histogram_tester.ExpectBucketCount(
      "Media.VideoCapture.Track.FrameDrop.DeviceCapture",
      media::VideoCaptureFrameDropReason::kDeviceClientFrameHasInvalidFormat,
      MediaStreamVideoTrack::kMaxConsecutiveFrameDropForSameReasonCount + 1);
  histogram_tester.ExpectBucketCount(
      "Media.VideoCapture.Track.FrameDrop.DeviceCapture",
      media::VideoCaptureFrameDropReason::kBufferPoolMaxBufferCountExceeded, 1);
}

TEST_F(MediaStreamVideoTrackTest, DroppedFrameCausesLogToBeEmitted) {
  constexpr media::VideoCaptureFrameDropReason kReason1 =
      static_cast<media::VideoCaptureFrameDropReason>(1);

  NiceMock<MockEmitLogMessageCb> emit_log_message_mock_;
  InitializeSource();
  auto* video_track = MediaStreamVideoTrack::From(CreateTrack());
  video_track->SetEmitLogMessageForTesting(emit_log_message_mock_.Callback());

  EXPECT_CALL(emit_log_message_mock_,
              EmitLogMessage(StrEq("Frame dropped with reason code 1.")))
      .Times(1);
  mock_source()->DropFrame(kReason1);
  DepleteIOCallbacks();
}

TEST_F(MediaStreamVideoTrackTest, DroppedFrameEmittedLogEventuallySuppressed) {
  constexpr media::VideoCaptureFrameDropReason kReason1 =
      static_cast<media::VideoCaptureFrameDropReason>(1);
  constexpr int kBeforeSuppressing =
      MediaStreamVideoTrack::kMaxEmittedLogsForDroppedFramesBeforeSuppressing;

  NiceMock<MockEmitLogMessageCb> emit_log_message_mock_;
  InitializeSource();
  auto* video_track = MediaStreamVideoTrack::From(CreateTrack());
  video_track->SetEmitLogMessageForTesting(emit_log_message_mock_.Callback());

  InSequence s;
  EXPECT_CALL(emit_log_message_mock_,
              EmitLogMessage(StrEq("Frame dropped with reason code 1.")))
      .Times(kBeforeSuppressing - 1);
  EXPECT_CALL(
      emit_log_message_mock_,
      EmitLogMessage(StrEq("Frame dropped with reason code 1. Additional logs "
                           "will be partially suppressed.")))
      .Times(1);
  EXPECT_CALL(emit_log_message_mock_, EmitLogMessage(_)).Times(0);

  // (Note that we drop N+1 times, and the last time is suppressed.)
  for (int i = 0; i < kBeforeSuppressing + 1; ++i) {
    mock_source()->DropFrame(kReason1);
  }
  DepleteIOCallbacks();
}

TEST_F(MediaStreamVideoTrackTest,
       DroppedFrameEmittedLogSuppressionOverOneReasonDoesNotAffectAnother) {
  constexpr media::VideoCaptureFrameDropReason kReason1 =
      static_cast<media::VideoCaptureFrameDropReason>(1);
  constexpr media::VideoCaptureFrameDropReason kReason2 =
      static_cast<media::VideoCaptureFrameDropReason>(2);
  constexpr int kBeforeSuppressing =
      MediaStreamVideoTrack::kMaxEmittedLogsForDroppedFramesBeforeSuppressing;

  NiceMock<MockEmitLogMessageCb> emit_log_message_mock_;
  InitializeSource();
  auto* video_track = MediaStreamVideoTrack::From(CreateTrack());
  video_track->SetEmitLogMessageForTesting(emit_log_message_mock_.Callback());

  // Emit reason-1 until it becomes suppressed.
  for (int i = 0; i < kBeforeSuppressing; ++i) {
    mock_source()->DropFrame(kReason1);
  }
  DepleteIOCallbacks();

  // As per a previous test, log emission for reason-1 will now be suppressed.
  // However, this does not affect reason-2, which is counted separately.
  InSequence s;
  EXPECT_CALL(emit_log_message_mock_,
              EmitLogMessage(StrEq("Frame dropped with reason code 2.")))
      .Times(kBeforeSuppressing - 1);
  EXPECT_CALL(
      emit_log_message_mock_,
      EmitLogMessage(StrEq("Frame dropped with reason code 2. Additional logs "
                           "will be partially suppressed.")))
      .Times(1);
  EXPECT_CALL(emit_log_message_mock_, EmitLogMessage(_)).Times(0);

  // (Note that we drop N+1 times, and the last time is suppressed.)
  for (int i = 0; i < kBeforeSuppressing; ++i) {
    mock_source()->DropFrame(kReason2);
  }
  DepleteIOCallbacks();
}

TEST_F(MediaStreamVideoTrackTest,
       DroppedFrameEmittedLogEmittedAtReducedFrequencyIfSuppressed) {
  constexpr media::VideoCaptureFrameDropReason kReason1 =
      static_cast<media::VideoCaptureFrameDropReason>(1);
  constexpr int kBeforeSuppressing =
      MediaStreamVideoTrack::kMaxEmittedLogsForDroppedFramesBeforeSuppressing;
  constexpr int kSuppressedFrequency =
      MediaStreamVideoTrack::kFrequencyForSuppressedLogs;

  NiceMock<MockEmitLogMessageCb> emit_log_message_mock_;
  InitializeSource();
  auto* video_track = MediaStreamVideoTrack::From(CreateTrack());
  video_track->SetEmitLogMessageForTesting(emit_log_message_mock_.Callback());

  // Emit reason-1 until it becomes suppressed.
  int drops = 0;
  for (; drops < kBeforeSuppressing; ++drops) {
    mock_source()->DropFrame(kReason1);
  }
  DepleteIOCallbacks();

  // Logs stay suppressed until we reach kSuppressedFrequency.
  EXPECT_CALL(emit_log_message_mock_, EmitLogMessage(_)).Times(0);
  for (; drops < kSuppressedFrequency - 1; ++drops) {
    mock_source()->DropFrame(kReason1);
  }

  // Suppressed logs still emitted, but at reduced frequency.
  EXPECT_CALL(emit_log_message_mock_,
              EmitLogMessage(StrEq("Frame dropped with reason code 1.")))
      .Times(1);
  mock_source()->DropFrame(kReason1);
  DepleteIOCallbacks();
}

TEST_P(MediaStreamVideoTrackTest, PropagatesContentHintType) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  MediaStreamVideoTrack::From(track)->SetContentHint(GetParam());
  EXPECT_EQ(sink.content_hint(), GetParam());
  sink.DisconnectFromTrack();
}

class MediaStreamVideoTrackEncodedTest : public MediaStreamVideoTrackTest {
 public:
  void InitializeSource() override {
    MediaStreamVideoTrackTest::InitializeSource();
    ON_CALL(*mock_source(), SupportsEncodedOutput).WillByDefault(Return(true));
  }
};

TEST_F(MediaStreamVideoTrackEncodedTest, ConnectEncodedSink) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  EXPECT_CALL(*mock_source(), OnEncodedSinkEnabled);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(false));
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true)).Times(0);
  EXPECT_CALL(*mock_source(), OnEncodedSinkDisabled).Times(0);
  sink.ConnectEncodedToTrack(track);
  Mock::VerifyAndClearExpectations(mock_source());
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest, DisconnectEncodedSink) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);
  EXPECT_CALL(*mock_source(), OnEncodedSinkDisabled);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true));
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest,
       CapturingLinkSecuredWithEncodedAndNormalTracks) {
  InitializeSource();
  MockMediaStreamVideoSink sink1;
  WebMediaStreamTrack track1 = CreateTrack();
  InSequence s;
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true));
  sink1.ConnectToTrack(track1);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(false));
  MockMediaStreamVideoSink sink2;
  WebMediaStreamTrack track2 = CreateTrack();
  sink2.ConnectEncodedToTrack(track2);
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(false));
  sink1.DisconnectFromTrack();
  EXPECT_CALL(*mock_source(), OnCapturingLinkSecured(true));
  sink2.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest, TransferOneEncodedVideoFrame) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);
  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnEncodedVideoFrame).WillOnce([&](base::TimeTicks) {
    std::move(quit_closure).Run();
  });
  mock_source()->DeliverEncodedVideoFrame(
      base::MakeRefCounted<MockEncodedVideoFrame>());
  run_loop.Run();
  Mock::VerifyAndClearExpectations(mock_source());
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest, SupportsEncodedDisableEnable) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);

  auto key_frame = base::MakeRefCounted<MockEncodedVideoFrame>();
  EXPECT_CALL(*key_frame, IsKeyFrame).WillRepeatedly(Return(true));
  auto delta_frame = base::MakeRefCounted<MockEncodedVideoFrame>();
  EXPECT_CALL(*delta_frame, IsKeyFrame).WillRepeatedly(Return(false));

  DeliverEncodedVideoFrameAndWait(key_frame, &sink);

  // Key frame when disabled -> shouldn't get dispatched
  MediaStreamVideoTrack::From(track)->SetEnabled(false);
  EXPECT_FALSE(sink.enabled());
  {
    EXPECT_CALL(sink, OnEncodedVideoFrame).Times(0);
    mock_source()->DeliverEncodedVideoFrame(key_frame);
    DepleteIOCallbacks();
  }

  // Delta frame when disabled -> shouldn't get dispatched until key frame
  // appears.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame);
  MediaStreamVideoTrack::From(track)->SetEnabled(true);
  EXPECT_TRUE(sink.enabled());
  {
    EXPECT_CALL(sink, OnEncodedVideoFrame).Times(0);
    mock_source()->DeliverEncodedVideoFrame(delta_frame);
    DepleteIOCallbacks();
  }

  // After a key frame things should be flowing again.
  DeliverEncodedVideoFrameAndWait(key_frame, &sink);
  DeliverEncodedVideoFrameAndWait(delta_frame, &sink);

  Mock::VerifyAndClearExpectations(mock_source());
  sink.DisconnectEncodedFromTrack();
}

TEST_P(MediaStreamVideoTrackEncodedTest, PropagatesContentHintType) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);
  MediaStreamVideoTrack::From(track)->SetContentHint(GetParam());
  EXPECT_EQ(sink.content_hint(), GetParam());
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackEncodedTest, SourceStopped) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track = CreateTrack();
  sink.ConnectEncodedToTrack(track);
  EXPECT_EQ(WebMediaStreamSource::kReadyStateLive, sink.state());

  mock_source()->StopSource();
  EXPECT_EQ(WebMediaStreamSource::kReadyStateEnded, sink.state());
  sink.DisconnectEncodedFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, DeliversConstraintsToKnownSinks) {
  InitializeSource();
  WebMediaStreamTrack track = CreateTrack();
  MockMediaStreamVideoSink sink1;
  EXPECT_CALL(sink1,
              OnVideoConstraintsChanged(Eq(std::nullopt), Eq(std::nullopt)));
  sink1.ConnectToTrack(track);
  MockMediaStreamVideoSink sink2;
  EXPECT_CALL(sink2,
              OnVideoConstraintsChanged(Eq(std::nullopt), Eq(std::nullopt)));
  sink2.ConnectToTrack(track);
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::From(track);
  Mock::VerifyAndClearExpectations(&sink1);
  Mock::VerifyAndClearExpectations(&sink2);

  EXPECT_CALL(sink1,
              OnVideoConstraintsChanged(Eq(std::nullopt), Eq(std::nullopt)));
  EXPECT_CALL(sink2,
              OnVideoConstraintsChanged(Eq(std::nullopt), Eq(std::nullopt)));
  native_track->SetTrackAdapterSettings(VideoTrackAdapterSettings());
  native_track->NotifyConstraintsConfigurationComplete();
  Mock::VerifyAndClearExpectations(&sink1);
  Mock::VerifyAndClearExpectations(&sink2);

  native_track->SetMinimumFrameRate(200);
  EXPECT_CALL(sink1,
              OnVideoConstraintsChanged(Optional(200.0), Eq(std::nullopt)));
  EXPECT_CALL(sink2,
              OnVideoConstraintsChanged(Optional(200.0), Eq(std::nullopt)));
  native_track->SetTrackAdapterSettings(VideoTrackAdapterSettings());
  native_track->NotifyConstraintsConfigurationComplete();
  Mock::VerifyAndClearExpectations(&sink1);
  Mock::VerifyAndClearExpectations(&sink2);

  EXPECT_CALL(sink1,
              OnVideoConstraintsChanged(Optional(200.0), Optional(300.0)));
  EXPECT_CALL(sink2,
              OnVideoConstraintsChanged(Optional(200.0), Optional(300.0)));
  VideoTrackAdapterSettings settings;
  settings.set_max_frame_rate(300);
  native_track->SetTrackAdapterSettings(settings);
  native_track->NotifyConstraintsConfigurationComplete();
  EXPECT_THAT(native_track->max_frame_rate(), testing::Optional(300));
  Mock::VerifyAndClearExpectations(&sink1);
  Mock::VerifyAndClearExpectations(&sink2);

  sink1.DisconnectFromTrack();
  sink2.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, DeliversConstraintsToNewSinks) {
  InitializeSource();
  WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::From(track);
  native_track->SetMinimumFrameRate(10);
  VideoTrackAdapterSettings settings;
  settings.set_max_frame_rate(20);
  native_track->SetTrackAdapterSettings(settings);
  native_track->NotifyConstraintsConfigurationComplete();
  EXPECT_THAT(native_track->max_frame_rate(), testing::Optional(20));

  MockMediaStreamVideoSink sink1;
  sink1.ConnectToTrack(track);
  Mock::VerifyAndClearExpectations(&sink1);

  MockMediaStreamVideoSink sink2;
  EXPECT_CALL(sink1, OnVideoConstraintsChanged).Times(0);
  EXPECT_CALL(sink2, OnVideoConstraintsChanged(Optional(10.0), Optional(20.0)));
  sink2.ConnectToTrack(track);
}

INSTANTIATE_TEST_SUITE_P(,
                         MediaStreamVideoTrackTest,
                         Values(ContentHintType::kVideoMotion,
                                ContentHintType::kVideoDetail,
                                ContentHintType::kVideoText));

INSTANTIATE_TEST_SUITE_P(,
                         MediaStreamVideoTrackEncodedTest,
                         Values(ContentHintType::kVideoMotion,
                                ContentHintType::kVideoDetail,
                                ContentHintType::kVideoText));

class MediaStreamVideoTrackRefreshFrameTimerTest
    : public MediaStreamVideoTrackTest {
 public:
  void SetUp() override { InitializeSource(); }
};

TEST_F(MediaStreamVideoTrackRefreshFrameTimerTest,
       SetMinFrameRateForScreenCastTrack) {
  // |RequestRefreshFrame| should be called exactly twice within kMinFrameRate
  // interval: First time from |AddSink| and second time from the refresh timer.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame).Times(2);
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track =
      CreateTrackWithSettings(VideoTrackAdapterSettings());
  auto* video_track = MediaStreamVideoTrack::From(track);
  video_track->SetMinimumFrameRate(kMinFrameRate);
  video_track->SetIsScreencastForTesting(true);

  sink.ConnectToTrack(track);
  test::RunDelayedTasks(base::Hertz(kMinFrameRate));

  EXPECT_TRUE(video_track->IsRefreshFrameTimerRunningForTesting());
  video_track->StopAndNotify(base::DoNothing());
  EXPECT_FALSE(video_track->IsRefreshFrameTimerRunningForTesting());
}

TEST_F(MediaStreamVideoTrackRefreshFrameTimerTest,
       SetMinFrameRateForNonScreenCastTrack) {
  // |RequestRefreshFrame| should only be called once from |AddSink| since
  // refresh frame timer is not running.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame).Times(1);
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track =
      CreateTrackWithSettings(VideoTrackAdapterSettings());

  auto* video_track = MediaStreamVideoTrack::From(track);
  video_track->SetMinimumFrameRate(kMinFrameRate);
  // Refresh frame timer will not be run when |is_screencast_| is false.
  video_track->SetIsScreencastForTesting(false);

  sink.ConnectToTrack(track);
  test::RunDelayedTasks(base::Hertz(kMinFrameRate));

  EXPECT_FALSE(video_track->IsRefreshFrameTimerRunningForTesting());
}

TEST_F(MediaStreamVideoTrackRefreshFrameTimerTest, RequiredRefreshRate) {
  // Sinks that have a required min frames per sec as 0 will not lead
  // to video track running the refresh frame timer.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame).Times(1);

  MockMediaStreamVideoSink sink;
  EXPECT_EQ(sink.GetRequiredMinFramesPerSec(), 0);

  WebMediaStreamTrack track =
      CreateTrackWithSettings(VideoTrackAdapterSettings());
  auto* video_track = MediaStreamVideoTrack::From(track);
  video_track->SetIsScreencastForTesting(true);

  sink.ConnectToTrack(track);
  test::RunDelayedTasks(base::Seconds(1));
}

TEST_F(MediaStreamVideoTrackRefreshFrameTimerTest,
       RequiredRefreshRateWebRTCSink) {
  // WebRTC sink has a required min frames per sec set to 1 so when we do
  // not have any min frame rate set on the video track, this required rate will
  // be used by the timer.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame).Times(2);

  WebMediaStreamTrack track = MediaStreamVideoTrack::CreateVideoTrack(
      mock_source(), WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      true);
  MediaStreamVideoTrack::From(track)->SetIsScreencastForTesting(true);

  Persistent<MediaStreamComponent> media_stream_component = *track;
  blink::MediaStreamVideoWebRtcSink webrtc_sink(
      media_stream_component,
      MakeGarbageCollected<MockPeerConnectionDependencyFactory>(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  EXPECT_EQ(webrtc_sink.GetRequiredMinFramesPerSec(), 1);

  test::RunDelayedTasks(base::Seconds(1));
}

TEST_F(MediaStreamVideoTrackRefreshFrameTimerTest,
       RequiredRefreshRateMultipleSinksAdded) {
  // |RequestRefreshFrame| will be called once for every sink added (twice here)
  // and third time from the refresh frame timer started by WebRTC sink. We will
  // pick the maximum of all the required refresh rates to run the timer.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame).Times(3);

  WebMediaStreamTrack track = MediaStreamVideoTrack::CreateVideoTrack(
      mock_source(), WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      true);
  MediaStreamVideoTrack::From(track)->SetIsScreencastForTesting(true);

  // First sink.
  MockMediaStreamVideoSink sink;
  EXPECT_EQ(sink.GetRequiredMinFramesPerSec(), 0);
  sink.ConnectToTrack(track);

  // Second sink.
  Persistent<MediaStreamComponent> media_stream_component = *track;
  blink::MediaStreamVideoWebRtcSink webrtc_sink(
      media_stream_component,
      MakeGarbageCollected<MockPeerConnectionDependencyFactory>(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  EXPECT_EQ(webrtc_sink.GetRequiredMinFramesPerSec(), 1);

  test::RunDelayedTasks(base::Seconds(1));
}

TEST_F(MediaStreamVideoTrackRefreshFrameTimerTest,
       RequiredRefreshRateMultipleSinksAddedAndRemoved) {
  // |RequestRefreshFrame| will be called once for every sink added (twice
  // here). The second sink (webrtc sink) does have a required min frames per
  // sec but it is removed.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame).Times(2);

  WebMediaStreamTrack track = MediaStreamVideoTrack::CreateVideoTrack(
      mock_source(), WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      true);
  MediaStreamVideoTrack::From(track)->SetIsScreencastForTesting(true);

  // First sink.
  MockMediaStreamVideoSink sink;
  EXPECT_EQ(sink.GetRequiredMinFramesPerSec(), 0);
  sink.ConnectToTrack(track);

  // Second sink added and then removed. The destructor for
  // MediaStreamVideoWebRtcSink calls DisconnectFromTrack.
  {
    Persistent<MediaStreamComponent> media_stream_component = *track;
    blink::MediaStreamVideoWebRtcSink webrtc_sink(
        media_stream_component,
        MakeGarbageCollected<MockPeerConnectionDependencyFactory>(),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
    EXPECT_EQ(webrtc_sink.GetRequiredMinFramesPerSec(), 1);
  }

  test::RunDelayedTasks(base::Seconds(1));
}

TEST_F(MediaStreamVideoTrackRefreshFrameTimerTest,
       NotifyConstraintsStartsTimerIfMinFpsIsSet) {
  // |RequestRefreshFrame| should be called exactly twice within kMinFrameRate
  // interval: First time from |NotifyConstraintsConfigurationComplete| and
  // second time from the refresh timer.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame).Times(2);
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track =
      CreateTrackWithSettings(VideoTrackAdapterSettings());
  auto* video_track = MediaStreamVideoTrack::From(track);

  video_track->SetIsScreencastForTesting(true);
  sink.ConnectToTrack(track);
  video_track->SetMinimumFrameRate(kMinFrameRate);
  video_track->NotifyConstraintsConfigurationComplete();

  test::RunDelayedTasks(base::Hertz(kMinFrameRate));

  EXPECT_TRUE(video_track->IsRefreshFrameTimerRunningForTesting());
  video_track->StopAndNotify(base::DoNothing());
  EXPECT_FALSE(video_track->IsRefreshFrameTimerRunningForTesting());
}

TEST_F(MediaStreamVideoTrackRefreshFrameTimerTest,
       NotifyConstraintsDontStartTimerIfMinFpsIsUnset) {
  // |RequestRefreshFrame| should only be called once from |AddSink| since
  // refresh frame timer is not running.
  EXPECT_CALL(*mock_source(), OnRequestRefreshFrame).Times(1);
  MockMediaStreamVideoSink sink;
  WebMediaStreamTrack track =
      CreateTrackWithSettings(VideoTrackAdapterSettings());
  auto* video_track = MediaStreamVideoTrack::From(track);

  video_track->SetIsScreencastForTesting(true);
  sink.ConnectToTrack(track);
  video_track->NotifyConstraintsConfigurationComplete();

  test::RunDelayedTasks(base::Hertz(kMinFrameRate));

  EXPECT_FALSE(video_track->IsRefreshFrameTimerRunningForTesting());
}

class MediaStreamVideoTrackCaptureVersionTest
    : public MediaStreamVideoTrackTest {
 public:
  using CaptureVersionCb = MockFunction<void()>;
  ~MediaStreamVideoTrackCaptureVersionTest() override = default;

  void SetUp() override {
    MediaStreamVideoTrackTest::SetUp();

    InitializeSource();
    track_ = CreateTrack();
    sink_.ConnectToTrack(track_);
    native_track_ = MediaStreamVideoTrack::From(track_);

    native_track_->SetSinkNotifyFrameDroppedCallback(
        &sink_, sink_.GetNotifyFrameDroppedCB());
  }

  void TearDown() override {
    sink_.DisconnectFromTrack();
    native_track_->StopAndNotify(base::DoNothing());
    native_track_ = nullptr;

    MediaStreamVideoTrackTest::TearDown();
  }

 protected:
  static base::OnceClosure AsClosure(CaptureVersionCb& cb) {
    return blink::BindOnce(&CaptureVersionCb::Call, base::Unretained(&cb));
  }

  static scoped_refptr<media::VideoFrame> MakeFrame(
      media::CaptureVersion capture_version = media::CaptureVersion()) {
    scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateBlackFrame(gfx::Size(600, 400));
    frame->metadata().capture_version = capture_version;
    return frame;
  }

  // Adds a callback for the given `capture_version`.
  // Returns a reference to that mock, in case the expectation needs to be
  // modified at a later time.
  CaptureVersionCb& AddCaptureVersionCallback(
      media::CaptureVersion capture_version) {
    native_track_->AddCaptureVersionCallback(capture_version,
                                             AsClosure(callbacks_[cb_count_]));
    return callbacks_[cb_count_++];
  }

  CaptureVersionCb& AddCaptureVersionCallback(uint32_t source,
                                              uint32_t sub_capture) {
    return AddCaptureVersionCallback(
        media::CaptureVersion(source, sub_capture));
  }

  void AddCaptureVersionCallbackAndAwaitPropagation(
      media::CaptureVersion capture_version) {
    // Add the callback.
    AddCaptureVersionCallback(capture_version);

    // Deliver a frame as a way of synchronizing with the other thread.
    // Note that we avoid EXPECT_CALL here, as this utility might be
    // used from a DEATH-test.
    base::RunLoop run_loop;
    ON_CALL(sink_, OnVideoFrame)
        .WillByDefault(RunOnceClosure(run_loop.QuitClosure()));
    mock_source()->DeliverVideoFrame(MakeFrame(media::CaptureVersion()));
    run_loop.Run();
  }

  void VerifyAndClearExpectations() {
    for (CaptureVersionCb& callback : callbacks_) {
      testing::Mock::VerifyAndClearExpectations(&callback);
    }
  }

  // Expects Call() to be invoked Times(1) on `expected_callbacks` and Times(0)
  // on all other callbacks.
  void ExpectCalls(const std::vector<CaptureVersionCb*>& expected_callbacks) {
    for (CaptureVersionCb& callback : callbacks_) {
      const int times = base::Contains(expected_callbacks, &callback) ? 1 : 0;
      EXPECT_CALL(callback, Call()).Times(times);
    }
  }

  void DeliverFrame(media::CaptureVersion capture_version) {
    DeliverVideoFrameAndWaitForRenderer(MakeFrame(capture_version), &sink_);
  }

  void DeliverFrame(uint32_t source, uint32_t sub_capture) {
    DeliverFrame(media::CaptureVersion(source, sub_capture));
  }

  void TryDeliverVideoFrameAndWaitForDrop(
      media::CaptureVersion capture_version) {
    base::RunLoop run_loop;
    base::RepeatingClosure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(sink_, OnNotifyFrameDropped)
        .WillOnce(RunOnceClosure(std::move(quit_closure)));
    mock_source()->DeliverVideoFrame(MakeFrame(capture_version));
    run_loop.Run();
  }

  void TryDeliverVideoFrameAndWaitForDrop(uint32_t source,
                                          uint32_t sub_capture) {
    TryDeliverVideoFrameAndWaitForDrop(
        media::CaptureVersion(source, sub_capture));
  }

  VideoFrameStats GetFrameStatsChange() {
    VideoFrameStats prev = last_frame_stats_;
    last_frame_stats_ = native_track_->GetVideoFrameStats();
    VideoFrameStats stats;
    stats.deliverable_frames =
        last_frame_stats_.deliverable_frames - prev.deliverable_frames;
    stats.discarded_frames =
        last_frame_stats_.discarded_frames - prev.discarded_frames;
    stats.dropped_frames =
        last_frame_stats_.dropped_frames - prev.dropped_frames;
    return stats;
  }

  bool IsExactlyOneFrameDelivered() {
    const VideoFrameStats change = GetFrameStatsChange();
    return change.deliverable_frames == 1u && change.discarded_frames == 0u &&
           change.dropped_frames == 0u;
  }

  bool IsExactlyOneFrameDropped() {
    const VideoFrameStats change = GetFrameStatsChange();
    return change.deliverable_frames == 0u && change.discarded_frames == 0u &&
           change.dropped_frames == 1u;
  }

  MockMediaStreamVideoSink sink_;
  WebMediaStreamTrack track_;
  raw_ptr<MediaStreamVideoTrack> native_track_;
  std::array<CaptureVersionCb, 20> callbacks_;
  size_t cb_count_ = 0;

  VideoFrameStats last_frame_stats_;
};

TEST_F(MediaStreamVideoTrackCaptureVersionTest,
       CorrectCaptureVersionCallbacksInvoked) {
  const uint32_t kSource = 333;
  const uint32_t kSubCapture = 666;

  // Older callbacks invoked.
  auto& old_1 = AddCaptureVersionCallback(kSource - 1, kSubCapture);
  auto& old_2 = AddCaptureVersionCallback(kSource - 1, kSubCapture - 1);
  auto& old_3 = AddCaptureVersionCallback(kSource, kSubCapture - 1);

  // Current callbacks invoked.
  auto& current = AddCaptureVersionCallback(kSource, kSubCapture);

  // Newer callbacks not invoked. (The named callbacks will be invoked later
  // in the test, and therefore a reference to them is kept.)
  auto& new_1 = AddCaptureVersionCallback(kSource, kSubCapture + 1);
  auto& new_2 = AddCaptureVersionCallback(kSource, kSubCapture + 2);
  auto& new_3 = AddCaptureVersionCallback(kSource + 1, kSubCapture);
  auto& new_4 = AddCaptureVersionCallback(kSource + 1, kSubCapture + 1);
  AddCaptureVersionCallback(kSource + 1, kSubCapture + 2);
  AddCaptureVersionCallback(kSource + 2, kSubCapture);
  AddCaptureVersionCallback(kSource + 2, kSubCapture + 1);

  // The following frame triggers the callback associated with the given
  // capture-version and older ones, and on none of the newer ones.
  ExpectCalls({&old_1, &old_2, &old_3, &current});
  DeliverFrame(media::CaptureVersion(kSource, kSubCapture));

  // The following frame is newer. It triggers the back on those callbacks
  // *which were not yet invoked*, and which are <= the new capture-version.
  VerifyAndClearExpectations();
  ExpectCalls({&new_1, &new_2, &new_3, &new_4});
  DeliverFrame(media::CaptureVersion(kSource + 1, kSubCapture + 1));
}

TEST_F(MediaStreamVideoTrackCaptureVersionTest, CallbackInvokedOnce) {
  const media::CaptureVersion capture_version(/*source=*/11,
                                              /*sub_capture*/ 22);

  auto& current = AddCaptureVersionCallback(capture_version);
  EXPECT_CALL(current, Call()).Times(1);

  DeliverFrame(capture_version);
  DeliverFrame(capture_version);
}

TEST_F(MediaStreamVideoTrackCaptureVersionTest, RemovedCallbackNotInvoked) {
  const media::CaptureVersion capture_version(/*source=*/11,
                                              /*sub_capture*/ 22);

  auto& current = AddCaptureVersionCallback(capture_version);
  EXPECT_CALL(current, Call()).Times(0);
  native_track_->RemoveCaptureVersionCallback(capture_version);

  DeliverFrame(capture_version);
}

// Mostly equivalent to `DeliverFrameWithSameVersion`, but checks the important
// common base case where the capture version has never been incremented.
TEST_F(MediaStreamVideoTrackCaptureVersionTest, DeliverBasicFrame) {
  DeliverFrame(media::CaptureVersion());
  EXPECT_TRUE(IsExactlyOneFrameDelivered());
}

TEST_F(MediaStreamVideoTrackCaptureVersionTest, DeliverFrameWithSameVersion) {
  for (size_t i = 0; i < 5; ++i) {
    DeliverFrame(media::CaptureVersion(/*source=*/123, /*sub_capture=*/456));
    EXPECT_TRUE(IsExactlyOneFrameDelivered());
  }
}

TEST_F(MediaStreamVideoTrackCaptureVersionTest, DeliverFramesWithNewerVersion) {
  const uint32_t kSource = 333;
  const uint32_t kSubCapture = 666;
  DeliverFrame(media::CaptureVersion(kSource, kSubCapture));
  ASSERT_TRUE(IsExactlyOneFrameDelivered());

  DeliverFrame(media::CaptureVersion(kSource, kSubCapture + 1));
  EXPECT_TRUE(IsExactlyOneFrameDelivered());

  DeliverFrame(media::CaptureVersion(kSource + 1, kSubCapture));
  EXPECT_TRUE(IsExactlyOneFrameDelivered());

  DeliverFrame(media::CaptureVersion(kSource + 1, kSubCapture + 1));
  EXPECT_TRUE(IsExactlyOneFrameDelivered());
}

TEST_F(MediaStreamVideoTrackCaptureVersionTest, DropFramesWithOlderVersion) {
  const uint32_t kSource = 333;
  const uint32_t kSubCapture = 666;
  DeliverFrame(media::CaptureVersion(kSource, kSubCapture));
  ASSERT_TRUE(IsExactlyOneFrameDelivered());

  TryDeliverVideoFrameAndWaitForDrop(kSource - 1, kSubCapture - 1);
  EXPECT_TRUE(IsExactlyOneFrameDropped());

  TryDeliverVideoFrameAndWaitForDrop(kSource - 1, kSubCapture);
  EXPECT_TRUE(IsExactlyOneFrameDropped());

  TryDeliverVideoFrameAndWaitForDrop(kSource, kSubCapture - 1);
  EXPECT_TRUE(IsExactlyOneFrameDropped());
}

TEST_F(MediaStreamVideoTrackCaptureVersionTest,
       CannotAddCaptureVersionCallbackZeroValue) {
  CaptureVersionCb callback;
  EXPECT_CHECK_DEATH(native_track_->AddCaptureVersionCallback(
      media::CaptureVersion(), AsClosure(callback)));
}

TEST_F(MediaStreamVideoTrackCaptureVersionTest,
       CannotRemoveCaptureVersionCallbackZeroValue) {
  EXPECT_CHECK_DEATH(
      native_track_->RemoveCaptureVersionCallback(media::CaptureVersion()));
}

TEST_F(MediaStreamVideoTrackCaptureVersionTest,
       CannotAddCaptureVersionCallbackTwice) {
  const media::CaptureVersion capture_version(/*source=*/123,
                                              /*sub_capture=*/456);

  // The first time does *not* cause a CHECK_DEATH.
  AddCaptureVersionCallbackAndAwaitPropagation(capture_version);

  // Must wait for the task to propagate to the other thread.
  EXPECT_CHECK_DEATH(
      AddCaptureVersionCallbackAndAwaitPropagation(capture_version));
}

TEST_F(MediaStreamVideoTrackCaptureVersionTest,
       RemoveCaptureVersionCallbackOfNeverAddedCallbackHandledGracefully) {
  const media::CaptureVersion capture_version(/*source=*/123,
                                              /*sub_capture=*/456);
  native_track_->RemoveCaptureVersionCallback(capture_version);

  // To avoid false positives, ensure a rendezvous with the FrameDeliverer by
  // delivering a frame.
  DeliverFrame(capture_version);
}

}  // namespace media_stream_video_track_test
}  // namespace blink
