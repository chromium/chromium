// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_video_renderer_sink.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_registry.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Lt;
using ::testing::Mock;

namespace blink {

class MediaStreamVideoRendererSinkTest : public testing::Test {
 public:
  MediaStreamVideoRendererSinkTest() {
    auto mock_source = std::make_unique<MockMediaStreamVideoSource>();
    mock_source_ = mock_source.get();
    media_stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8("dummy_source_id"), MediaStreamSource::kTypeVideo,
        String::FromUTF8("dummy_source_name"), false /* remote */,
        std::move(mock_source));
    WebMediaStreamTrack web_track = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_, WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
        true);
    media_stream_component_ = *web_track;
    mock_source_->StartMockedSource();
    base::RunLoop().RunUntilIdle();

    media_stream_video_renderer_sink_ =
        base::MakeRefCounted<MediaStreamVideoRendererSink>(
            media_stream_component_,
            ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                &MediaStreamVideoRendererSinkTest::RepaintCallback,
                CrossThreadUnretained(this))),
            Platform::Current()->GetIOTaskRunner(),
            scheduler::GetSingleThreadTaskRunnerForTesting());
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(IsInStoppedState());
  }

  MediaStreamVideoRendererSinkTest(const MediaStreamVideoRendererSinkTest&) =
      delete;
  MediaStreamVideoRendererSinkTest& operator=(
      const MediaStreamVideoRendererSinkTest&) = delete;

  void TearDown() override {
    media_stream_video_renderer_sink_ = nullptr;
    media_stream_source_ = nullptr;
    media_stream_component_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();

    // Let the message loop run to finish destroying the pool.
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD1(RepaintCallback, void(scoped_refptr<media::VideoFrame>));

  bool IsInStartedState() const {
    RunIOUntilIdle();
    return media_stream_video_renderer_sink_->GetStateForTesting() ==
           MediaStreamVideoRendererSink::kStarted;
  }
  bool IsInStoppedState() const {
    RunIOUntilIdle();
    return media_stream_video_renderer_sink_->GetStateForTesting() ==
           MediaStreamVideoRendererSink::kStopped;
  }
  bool IsInPausedState() const {
    RunIOUntilIdle();
    return media_stream_video_renderer_sink_->GetStateForTesting() ==
           MediaStreamVideoRendererSink::kPaused;
  }

  void OnVideoFrame(scoped_refptr<media::VideoFrame> frame) {
    mock_source_->DeliverVideoFrame(frame);
    base::RunLoop().RunUntilIdle();

    RunIOUntilIdle();
  }

  test::TaskEnvironment task_environment_;
  scoped_refptr<MediaStreamVideoRendererSink> media_stream_video_renderer_sink_;

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  Persistent<MediaStreamComponent> media_stream_component_;

 private:
  void RunIOUntilIdle() const {
    // |media_stream_component_| uses video task runner to send frames to sinks.
    // Make sure that tasks on video task runner are completed before moving on.
    base::RunLoop run_loop;
    Platform::Current()->GetIOTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::BindOnce([] {}), run_loop.QuitClosure());
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }

  Persistent<MediaStreamSource> media_stream_source_;
  raw_ptr<MockMediaStreamVideoSource, DanglingUntriaged> mock_source_;
};

// Checks that the initialization-destruction sequence works fine.
TEST_F(MediaStreamVideoRendererSinkTest, StartStop) {
  EXPECT_TRUE(IsInStoppedState());

  media_stream_video_renderer_sink_->Start();
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Pause();
  EXPECT_TRUE(IsInPausedState());

  media_stream_video_renderer_sink_->Resume();
  EXPECT_TRUE(IsInStartedState());

  media_stream_video_renderer_sink_->Stop();
  EXPECT_TRUE(IsInStoppedState());
}

// Sends 2 frames and expect them as WebM contained encoded data in writeData().
TEST_F(MediaStreamVideoRendererSinkTest, EncodeVideoFrames) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  EXPECT_CALL(*this, RepaintCallback(video_frame)).Times(1);
  OnVideoFrame(video_frame);

  media_stream_video_renderer_sink_->Stop();
}

class MediaStreamVideoRendererSinkTransparencyTest
    : public MediaStreamVideoRendererSinkTest {
 public:
  MediaStreamVideoRendererSinkTransparencyTest() {
    media_stream_video_renderer_sink_ =
        base::MakeRefCounted<MediaStreamVideoRendererSink>(
            media_stream_component_,
            ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
                &MediaStreamVideoRendererSinkTransparencyTest::
                    VerifyTransparentFrame,
                CrossThreadUnretained(this))),
            Platform::Current()->GetIOTaskRunner(),
            scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  void VerifyTransparentFrame(scoped_refptr<media::VideoFrame> frame) {
    EXPECT_EQ(media::PIXEL_FORMAT_I420A, frame->format());
  }
};

TEST_F(MediaStreamVideoRendererSinkTransparencyTest, SendTransparentFrame) {
  media_stream_video_renderer_sink_->Start();

  InSequence s;
  const gfx::Size kSize(10, 10);
  const base::TimeDelta kTimestamp = base::TimeDelta();
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_I420A, kSize,
                                     gfx::Rect(kSize), kSize, kTimestamp);
  OnVideoFrame(video_frame);
  base::RunLoop().RunUntilIdle();

  media_stream_video_renderer_sink_->Stop();
}

}  // namespace blink
