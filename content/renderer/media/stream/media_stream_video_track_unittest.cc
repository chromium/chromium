// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_checker_impl.h"
#include "content/child/child_process.h"
#include "content/renderer/media/stream/media_stream_video_track.h"
#include "content/renderer/media/stream/mock_media_stream_video_sink.h"
#include "content/renderer/media/stream/mock_media_stream_video_source.h"
#include "content/renderer/media/stream/video_track_adapter.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"

namespace content {

const uint8_t kBlackValue = 0x00;
const uint8_t kColorValue = 0xAB;
const int kMockSourceWidth = 640;
const int kMockSourceHeight = 480;

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MediaStreamVideoTrackTest : public ::testing::Test {
 public:
  MediaStreamVideoTrackTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        mock_source_(nullptr),
        source_started_(false) {}

  ~MediaStreamVideoTrackTest() override {}

  void TearDown() override {
    blink_source_.Reset();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  void DeliverVideoFrameAndWaitForRenderer(MockMediaStreamVideoSink* sink) {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    EXPECT_CALL(*sink, OnVideoFrame())
        .WillOnce(RunClosure(std::move(quit_closure)));
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateColorFrame(
            gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                      MediaStreamVideoSource::kDefaultHeight),
            kColorValue, kColorValue, kColorValue, base::TimeDelta());
    mock_source()->DeliverVideoFrame(frame);
    run_loop.Run();
  }

 protected:
  void InitializeSource() {
    blink_source_.Reset();
    mock_source_ = new MockMediaStreamVideoSource(
        media::VideoCaptureFormat(
            gfx::Size(kMockSourceWidth, kMockSourceHeight), 30.0,
            media::PIXEL_FORMAT_I420),
        false);
    blink_source_.Initialize(blink::WebString::FromASCII("dummy_source_id"),
                             blink::WebMediaStreamSource::kTypeVideo,
                             blink::WebString::FromASCII("dummy_source_name"),
                             false /* remote */);
    blink_source_.SetExtraData(mock_source_);
  }

  // Create a track that's associated with |mock_source_|.
  blink::WebMediaStreamTrack CreateTrack() {
    const bool enabled = true;
    blink::WebMediaStreamTrack track = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, MediaStreamSource::ConstraintsCallback(), enabled);
    if (!source_started_) {
      mock_source_->StartMockedSource();
      source_started_ = true;
    }
    return track;
  }

  // Create a track that's associated with |mock_source_| and has the given
  // |adapter_settings|.
  blink::WebMediaStreamTrack CreateTrackWithSettings(
      const VideoTrackAdapterSettings& adapter_settings) {
    const bool enabled = true;
    blink::WebMediaStreamTrack track = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, adapter_settings, base::Optional<bool>(), false, 0.0,
        MediaStreamSource::ConstraintsCallback(), enabled);
    if (!source_started_) {
      mock_source_->StartMockedSource();
      source_started_ = true;
    }
    return track;
  }

  void UpdateVideoSourceToRespondToRequestRefreshFrame() {
    blink_source_.Reset();
    mock_source_ = new MockMediaStreamVideoSource(
        media::VideoCaptureFormat(
            gfx::Size(kMockSourceWidth, kMockSourceHeight), 30.0,
            media::PIXEL_FORMAT_I420),
        true);
    blink_source_.Initialize(blink::WebString::FromASCII("dummy_source_id"),
                             blink::WebMediaStreamSource::kTypeVideo,
                             blink::WebString::FromASCII("dummy_source_name"),
                             false /* remote */);
    blink_source_.SetExtraData(mock_source_);
  }

  MockMediaStreamVideoSource* mock_source() { return mock_source_; }
  const blink::WebMediaStreamSource& blink_source() const {
    return blink_source_;
  }

 private:
  // The ScopedTaskEnvironment prevents the ChildProcess from leaking a
  // TaskScheduler.
  const base::test::ScopedTaskEnvironment scoped_task_environment_;
  const ChildProcess child_process_;
  blink::WebMediaStreamSource blink_source_;
  // |mock_source_| is owned by |webkit_source_|.
  MockMediaStreamVideoSource* mock_source_;
  bool source_started_;
};

TEST_F(MediaStreamVideoTrackTest, AddAndRemoveSink) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);

  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());

  DeliverVideoFrameAndWaitForRenderer(&sink);

  sink.DisconnectFromTrack();

  scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(
          gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                    MediaStreamVideoSource::kDefaultHeight));
  mock_source()->DeliverVideoFrame(frame);
  // Wait for the IO thread to complete delivering frames.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, sink.number_of_frames());
}

class CheckThreadHelper {
 public:
  CheckThreadHelper(base::Closure callback, bool* correct)
      : callback_(callback),
        correct_(correct) {
  }

  ~CheckThreadHelper() {
    *correct_ = thread_checker_.CalledOnValidThread();
    callback_.Run();
  }

 private:
  base::Closure callback_;
  bool* correct_;
  base::ThreadCheckerImpl thread_checker_;
};

void CheckThreadVideoFrameReceiver(
    CheckThreadHelper* helper,
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks estimated_capture_time) {
  // Do nothing.
}

// Checks that the callback given to the track is reset on the right thread.
TEST_F(MediaStreamVideoTrackTest, ResetCallbackOnThread) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();

  base::RunLoop run_loop;
  bool correct = false;
  sink.ConnectToTrackWithCallback(
      track,
      base::Bind(&CheckThreadVideoFrameReceiver,
                 base::Owned(new CheckThreadHelper(run_loop.QuitClosure(),
                                                   &correct))));
  sink.DisconnectFromTrack();
  run_loop.Run();
  EXPECT_TRUE(correct) << "Not called on correct thread.";
}

TEST_F(MediaStreamVideoTrackTest, SetEnabled) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);

  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::GetVideoTrack(track);

  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(1, sink.number_of_frames());
  EXPECT_EQ(kColorValue, *sink.last_frame()->data(media::VideoFrame::kYPlane));

  video_track->SetEnabled(false);
  EXPECT_FALSE(sink.enabled());

  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(2, sink.number_of_frames());
  EXPECT_EQ(kBlackValue, *sink.last_frame()->data(media::VideoFrame::kYPlane));

  video_track->SetEnabled(true);
  EXPECT_TRUE(sink.enabled());
  DeliverVideoFrameAndWaitForRenderer(&sink);
  EXPECT_EQ(3, sink.number_of_frames());
  EXPECT_EQ(kColorValue, *sink.last_frame()->data(media::VideoFrame::kYPlane));
  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, SourceStopped) {
  InitializeSource();
  MockMediaStreamVideoSink sink;
  blink::WebMediaStreamTrack track = CreateTrack();
  sink.ConnectToTrack(track);
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive, sink.state());

  mock_source()->StopSource();
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded, sink.state());
  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, StopLastTrack) {
  InitializeSource();
  MockMediaStreamVideoSink sink1;
  blink::WebMediaStreamTrack track1 = CreateTrack();
  sink1.ConnectToTrack(track1);
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive, sink1.state());

  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive,
            blink_source().GetReadyState());

  MockMediaStreamVideoSink sink2;
  blink::WebMediaStreamTrack track2 = CreateTrack();
  sink2.ConnectToTrack(track2);
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive, sink2.state());

  MediaStreamVideoTrack* const native_track1 =
      MediaStreamVideoTrack::GetVideoTrack(track1);
  native_track1->Stop();
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded, sink1.state());
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateLive,
            blink_source().GetReadyState());
  sink1.DisconnectFromTrack();

  MediaStreamVideoTrack* const native_track2 =
        MediaStreamVideoTrack::GetVideoTrack(track2);
  native_track2->Stop();
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded, sink2.state());
  EXPECT_EQ(blink::WebMediaStreamSource::kReadyStateEnded,
            blink_source().GetReadyState());
  sink2.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, CheckTrackRequestsFrame) {
  InitializeSource();
  UpdateVideoSourceToRespondToRequestRefreshFrame();
  blink::WebMediaStreamTrack track = CreateTrack();

  // Add sink and expect to get a frame.
  MockMediaStreamVideoSink sink;
  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();
  EXPECT_CALL(sink, OnVideoFrame())
      .WillOnce(RunClosure(std::move(quit_closure)));
  sink.ConnectToTrack(track);
  run_loop.Run();
  EXPECT_EQ(1, sink.number_of_frames());

  sink.DisconnectFromTrack();
}

TEST_F(MediaStreamVideoTrackTest, GetSettings) {
  InitializeSource();
  blink::WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  blink::WebMediaStreamTrack::Settings settings;
  native_track->GetSettings(settings);
  // These values come straight from the mock video track implementation.
  EXPECT_EQ(640, settings.width);
  EXPECT_EQ(480, settings.height);
  EXPECT_EQ(30.0, settings.frame_rate);
  EXPECT_EQ(blink::WebMediaStreamTrack::FacingMode::kNone,
            settings.facing_mode);
}

TEST_F(MediaStreamVideoTrackTest, GetSettingsWithAdjustment) {
  InitializeSource();
  const int kAdjustedWidth = 600;
  const int kAdjustedHeight = 400;
  const double kAdjustedFrameRate = 20.0;
  VideoTrackAdapterSettings adapter_settings(
      gfx::Size(kAdjustedWidth, kAdjustedHeight), 0.0, 10000.0,
      kAdjustedFrameRate);
  blink::WebMediaStreamTrack track = CreateTrackWithSettings(adapter_settings);
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  blink::WebMediaStreamTrack::Settings settings;
  native_track->GetSettings(settings);
  EXPECT_EQ(kAdjustedWidth, settings.width);
  EXPECT_EQ(kAdjustedHeight, settings.height);
  EXPECT_EQ(kAdjustedFrameRate, settings.frame_rate);
  EXPECT_EQ(blink::WebMediaStreamTrack::FacingMode::kNone,
            settings.facing_mode);
}

TEST_F(MediaStreamVideoTrackTest, GetSettingsStopped) {
  InitializeSource();
  blink::WebMediaStreamTrack track = CreateTrack();
  MediaStreamVideoTrack* const native_track =
      MediaStreamVideoTrack::GetVideoTrack(track);
  native_track->Stop();
  blink::WebMediaStreamTrack::Settings settings;
  native_track->GetSettings(settings);
  EXPECT_EQ(-1, settings.width);
  EXPECT_EQ(-1, settings.height);
  EXPECT_EQ(-1, settings.frame_rate);
  EXPECT_EQ(blink::WebMediaStreamTrack::FacingMode::kNone,
            settings.facing_mode);
  EXPECT_TRUE(settings.device_id.IsNull());
}

}  // namespace content
