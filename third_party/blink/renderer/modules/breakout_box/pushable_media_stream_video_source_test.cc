// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_sink.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

class FakeMediaStreamVideoSink : public MediaStreamVideoSink {
 public:
  FakeMediaStreamVideoSink(base::TimeTicks* capture_time,
                           media::VideoFrameMetadata* metadata,
                           gfx::Size* natural_size,
                           base::OnceClosure got_frame_cb)
      : capture_time_(capture_time),
        metadata_(metadata),
        natural_size_(natural_size),
        got_frame_cb_(std::move(got_frame_cb)) {}

  void ConnectToTrack(const WebMediaStreamTrack& track) {
    MediaStreamVideoSink::ConnectToTrack(
        track,
        ConvertToBaseRepeatingCallback(
            CrossThreadBindRepeating(&FakeMediaStreamVideoSink::OnVideoFrame,
                                     WTF::CrossThreadUnretained(this))),
        MediaStreamVideoSink::IsSecure::kYes,
        MediaStreamVideoSink::UsesAlpha::kDefault);
  }

  void DisconnectFromTrack() { MediaStreamVideoSink::DisconnectFromTrack(); }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame,
                    base::TimeTicks capture_time) {
    *capture_time_ = capture_time;
    *metadata_ = frame->metadata();
    *natural_size_ = frame->natural_size();
    std::move(got_frame_cb_).Run();
  }

 private:
  const raw_ptr<base::TimeTicks> capture_time_;
  const raw_ptr<media::VideoFrameMetadata> metadata_;
  const raw_ptr<gfx::Size> natural_size_;
  base::OnceClosure got_frame_cb_;
};

MediaStreamSource* CreateConnectedMediaStreamSource(
    std::unique_ptr<MediaStreamVideoSource> video_source) {
  MediaStreamSource* media_stream_source =
      MakeGarbageCollected<MediaStreamSource>(
          "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
          false /* remote */, std::move(video_source));
  return media_stream_source;
}

WebMediaStreamTrack StartVideoSource(MediaStreamVideoSource* video_source) {
  return MediaStreamVideoTrack::CreateVideoTrack(
      video_source, MediaStreamVideoSource::ConstraintsOnceCallback(),
      /*enabled=*/true);
}

}  // namespace

class PushableMediaStreamVideoSourceTest : public testing::Test {
 public:
  PushableMediaStreamVideoSourceTest() {
    auto pushable_video_source =
        std::make_unique<PushableMediaStreamVideoSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting());
    pushable_video_source_ = pushable_video_source.get();
    stream_source_ =
        CreateConnectedMediaStreamSource(std::move(pushable_video_source));
  }

  void TearDown() override {
    stream_source_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  WebMediaStreamTrack StartSource() {
    return StartVideoSource(pushable_video_source_);
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  Persistent<MediaStreamSource> stream_source_;
  raw_ptr<PushableMediaStreamVideoSource, DanglingUntriaged>
      pushable_video_source_;
};

TEST_F(PushableMediaStreamVideoSourceTest, StartAndStop) {
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(pushable_video_source_->IsRunning());

  WebMediaStreamTrack track = StartSource();
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_TRUE(pushable_video_source_->IsRunning());

  // If the pushable source stops, the MediaStreamSource should stop.
  pushable_video_source_->StopSource();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  EXPECT_FALSE(pushable_video_source_->IsRunning());
}

TEST_F(PushableMediaStreamVideoSourceTest, FramesPropagateToSink) {
  WebMediaStreamTrack track = StartSource();
  base::RunLoop run_loop;
  base::TimeTicks reference_capture_time = base::TimeTicks::Now();
  base::TimeTicks capture_time;
  media::VideoFrameMetadata metadata;
  gfx::Size natural_size;
  FakeMediaStreamVideoSink fake_sink(
      &capture_time, &metadata, &natural_size,
      base::BindPostTaskToCurrentDefault(run_loop.QuitClosure()));
  fake_sink.ConnectToTrack(track);
  const scoped_refptr<media::VideoFrame> frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(100, 50));
  frame->metadata().frame_rate = 30.0;

  pushable_video_source_->PushFrame(frame, reference_capture_time);
  run_loop.Run();

  fake_sink.DisconnectFromTrack();
  EXPECT_EQ(reference_capture_time, capture_time);
  EXPECT_EQ(30.0, *metadata.frame_rate);
  EXPECT_EQ(natural_size.width(), 100);
  EXPECT_EQ(natural_size.height(), 50);
}

}  // namespace blink
