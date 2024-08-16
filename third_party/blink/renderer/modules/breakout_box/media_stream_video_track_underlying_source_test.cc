// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_video_track_underlying_source.h"

#include <optional>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "media/base/video_frame_metadata.h"
#include "media/capture/video/video_capture_buffer_pool_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/breakout_box/stream_test_utils.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame_monitor.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

using testing::_;

namespace blink {

class MediaStreamVideoTrackUnderlyingSourceTest : public testing::Test {
 public:
  MediaStreamVideoTrackUnderlyingSourceTest()
      : pushable_video_source_(new PushableMediaStreamVideoSource(
            scheduler::GetSingleThreadTaskRunnerForTesting())),
        media_stream_source_(MakeGarbageCollected<MediaStreamSource>(
            "dummy_source_id",
            MediaStreamSource::kTypeVideo,
            "dummy_source_name",
            false /* remote */,
            base::WrapUnique(pushable_video_source_.get()))) {}

  ~MediaStreamVideoTrackUnderlyingSourceTest() override {
    RunIOUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamTrack* CreateTrack(ExecutionContext* execution_context) {
    return MakeGarbageCollected<MediaStreamTrackImpl>(
        execution_context,
        MediaStreamVideoTrack::CreateVideoTrack(
            pushable_video_source_,
            MediaStreamVideoSource::ConstraintsOnceCallback(),
            /*enabled=*/true));
  }

  MediaStreamVideoTrackUnderlyingSource* CreateSource(ScriptState* script_state,
                                                      MediaStreamTrack* track,
                                                      wtf_size_t buffer_size) {
    return MakeGarbageCollected<MediaStreamVideoTrackUnderlyingSource>(
        script_state, track->Component(), nullptr, buffer_size);
  }

  MediaStreamVideoTrackUnderlyingSource* CreateSource(ScriptState* script_state,
                                                      MediaStreamTrack* track) {
    return CreateSource(script_state, track, 1u);
  }

 private:
  void RunIOUntilIdle() const {
    // Make sure that tasks on IO thread are completed before moving on.
    base::RunLoop run_loop;
    Platform::Current()->GetIOTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::BindOnce([] {}), run_loop.QuitClosure());
    run_loop.Run();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void PushFrame(
      const std::optional<base::TimeDelta>& timestamp = std::nullopt) {
    const scoped_refptr<media::VideoFrame> frame =
        media::VideoFrame::CreateBlackFrame(gfx::Size(10, 5));
    if (timestamp) {
      frame->set_timestamp(*timestamp);
    }
    pushable_video_source_->PushFrame(frame, base::TimeTicks());
    RunIOUntilIdle();
  }

  static MediaStreamSource* CreateDevicePushableSource(
      const std::string& device_id) {
    auto pushable_video_source =
        std::make_unique<PushableMediaStreamVideoSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting());
    PushableMediaStreamVideoSource* pushable_video_source_ptr =
        pushable_video_source.get();
    auto* media_stream_source = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeVideo, "dummy_source_name",
        false /* remote */, std::move(pushable_video_source));
    MediaStreamDevice device(mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE,
                             device_id, "My window device");
    pushable_video_source_ptr->SetDevice(device);

    return media_stream_source;
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  const raw_ptr<PushableMediaStreamVideoSource> pushable_video_source_;
  const Persistent<MediaStreamSource> media_stream_source_;
};

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       VideoFrameFlowsThroughStreamAndCloses) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  NonThrowableExceptionState exception_state;
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, exception_state);

  ScriptPromiseTester read_tester(script_state,
                                  reader->read(script_state, exception_state));
  EXPECT_FALSE(read_tester.IsFulfilled());
  PushFrame();
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       CancelStreamDisconnectsFromTrack) {
  V8TestingScope v8_scope;
  MediaStreamTrack* track = CreateTrack(v8_scope.GetExecutionContext());
  MediaStreamVideoTrack* video_track =
      MediaStreamVideoTrack::From(track->Component());
  // Initially the track has no sinks.
  EXPECT_EQ(video_track->CountSinks(), 0u);

  auto* source = CreateSource(v8_scope.GetScriptState(), track);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      v8_scope.GetScriptState(), source, 0);

  // The stream is a sink to the track.
  EXPECT_EQ(video_track->CountSinks(), 1u);

  NonThrowableExceptionState exception_state;
  stream->cancel(v8_scope.GetScriptState(), exception_state);

  // Canceling the stream disconnects it from the track.
  EXPECT_EQ(video_track->CountSinks(), 0u);
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       DropOldFramesWhenQueueIsFull) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  const wtf_size_t buffer_size = 5;
  auto* source = CreateSource(script_state, track, buffer_size);
  EXPECT_EQ(source->MaxQueueSize(), buffer_size);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  // Add a sink to the track to make it possible to wait until a pushed frame
  // is delivered to sinks, including |source|, which is a sink of the track.
  MockMediaStreamVideoSink mock_sink;
  mock_sink.ConnectToTrack(WebMediaStreamTrack(source->Track()));
  auto push_frame_sync = [&mock_sink, this](const base::TimeDelta timestamp) {
    base::RunLoop sink_loop;
    EXPECT_CALL(mock_sink, OnVideoFrame(_))
        .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
    PushFrame(timestamp);
    sink_loop.Run();
  };

  for (wtf_size_t i = 0; i < buffer_size; ++i) {
    base::TimeDelta timestamp = base::Seconds(i);
    push_frame_sync(timestamp);
  }

  // Push another frame while the queue is full.
  // EXPECT_EQ(queue.size(), buffer_size);
  push_frame_sync(base::Seconds(buffer_size));

  // Since the queue was full, the oldest frame from the queue (timestamp 0)
  // should have been dropped.
  NonThrowableExceptionState exception_state;
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, exception_state);
  for (wtf_size_t i = 1; i <= buffer_size; ++i) {
    VideoFrame* video_frame =
        ReadObjectFromStream<VideoFrame>(v8_scope, reader);
    EXPECT_EQ(video_frame->frame()->timestamp(), base::Seconds(i));
  }

  // Pulling causes a pending pull since there are no frames available for
  // reading.
  EXPECT_EQ(source->NumPendingPullsForTesting(), 0);
  source->Pull(script_state, ASSERT_NO_EXCEPTION);
  EXPECT_EQ(source->NumPendingPullsForTesting(), 1);

  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest, QueueSizeCannotBeZero) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track, 0u);
  // Queue size is always at least 1, even if 0 is requested.
  EXPECT_EQ(source->MaxQueueSize(), 1u);
  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest, PlatformSourceAliveAfterGC) {
  Persistent<MediaStreamComponent> component;
  {
    V8TestingScope v8_scope;
    auto* track = CreateTrack(v8_scope.GetExecutionContext());
    component = track->Component();
    auto* source = CreateSource(v8_scope.GetScriptState(), track, 0u);
    ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                    source, 0);
    // |source| is a sink of |track|.
    EXPECT_TRUE(source->Track());
  }
  blink::WebHeap::CollectAllGarbageForTesting();
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest, CloseOnContextDestroyed) {
  MediaStreamVideoTrackUnderlyingSource* source = nullptr;
  {
    V8TestingScope v8_scope;
    ScriptState* script_state = v8_scope.GetScriptState();
    auto* track = CreateTrack(v8_scope.GetExecutionContext());
    source = CreateSource(script_state, track, 0u);
    EXPECT_FALSE(source->IsClosed());
    // Create a stream so that |source| starts.
    auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
        v8_scope.GetScriptState(), source, 0);
    EXPECT_FALSE(source->IsClosed());
    EXPECT_FALSE(stream->IsClosed());
  }
  EXPECT_TRUE(source->IsClosed());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest, CloseBeforeStart) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track, 0u);
  EXPECT_FALSE(source->IsClosed());
  source->Close();
  EXPECT_TRUE(source->IsClosed());
  // Create a stream so that the start method of |source| runs.
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      v8_scope.GetScriptState(), source, 0);
  EXPECT_TRUE(source->IsClosed());
  EXPECT_TRUE(stream->IsClosed());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       DeviceIdAndMaxFrameCountForMonitoring) {
  using M = MediaStreamVideoTrackUnderlyingSource;
  const std::string window_id = "window:a-window";
  const std::string screen_id = "screen:a-screen";
  const std::string tab_id = "web-contents-media-stream://5:1";
  const std::string camera_id = "my-camera";
  const std::string mic_id = "my-mic";

  MediaStreamDevice device;
  device.type = mojom::MediaStreamType::NO_SERVICE;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);

  device.id = mic_id;
  device.type = mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);

  device.id = tab_id;
  device.type = mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);
  device.type = mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);

  device.type = mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE;
  device.id = screen_id;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);
  device.id = window_id;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);

  device.type = mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE;
  device.id = screen_id;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);
  device.id = window_id;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);
  device.id = tab_id;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);

  device.type = mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB;
  device.id = tab_id;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);

  // Camera capture is subject to monitoring.
  device.type = mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE;
  device.id = camera_id;
  EXPECT_FALSE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device),
            static_cast<size_t>(
                std::max(media::kVideoCaptureDefaultMaxBufferPoolSize / 2,
                         media::DeviceVideoCaptureMaxBufferPoolSize() / 3)));

  // Screen and Window capture with the desktop capture extension API are
  // subject to monitoring.
  device.type = mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
  device.id = screen_id;
  EXPECT_FALSE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(
      M::GetFramePoolSize(device),
      static_cast<size_t>(media::kVideoCaptureDefaultMaxBufferPoolSize / 2));
  device.id = window_id;
  EXPECT_FALSE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(
      M::GetFramePoolSize(device),
      static_cast<size_t>(media::kVideoCaptureDefaultMaxBufferPoolSize / 2));

  // Screen and Window capture with getDisplayMedia are subject to monitoring,
  // but not tab capture.
  device.type = mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE;
  device.id = screen_id;
  EXPECT_FALSE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(
      M::GetFramePoolSize(device),
      static_cast<size_t>(media::kVideoCaptureDefaultMaxBufferPoolSize / 2));
  device.id = window_id;
  EXPECT_FALSE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(
      M::GetFramePoolSize(device),
      static_cast<size_t>(media::kVideoCaptureDefaultMaxBufferPoolSize / 2));
  device.id = tab_id;
  EXPECT_TRUE(M::GetDeviceIdForMonitoring(device).empty());
  EXPECT_EQ(M::GetFramePoolSize(device), 0u);
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest, FrameLimiter) {
  const std::string device_id = "window:my-window";
  auto* media_stream_source = CreateDevicePushableSource(device_id);
  auto* platform_video_source =
      static_cast<blink::PushableMediaStreamVideoSource*>(
          media_stream_source->GetPlatformSource());
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(),
      MediaStreamVideoTrack::CreateVideoTrack(
          platform_video_source,
          MediaStreamVideoSource::ConstraintsOnceCallback(),
          /*enabled=*/true));
  // Use a large buffer so that the effective buffer size is guaranteed to be
  // the one set by frame monitoring.
  auto* source = CreateSource(
      script_state, track,
      MediaStreamVideoTrackUnderlyingSource::kMaxMonitoredFrameCount);
  const wtf_size_t max_frame_count =
      MediaStreamVideoTrackUnderlyingSource::GetFramePoolSize(
          platform_video_source->device());

  // This test assumes that |max_frame_count| is 2, for simplicity.
  ASSERT_EQ(max_frame_count, 2u);

  VideoFrameMonitor& monitor = VideoFrameMonitor::Instance();
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      v8_scope.GetScriptState(), source, 0);

  // Add a sink to the track to make it possible to wait until a pushed frame
  // is delivered to sinks, including |source|, which is a sink of the track.
  MockMediaStreamVideoSink mock_sink;
  mock_sink.ConnectToTrack(WebMediaStreamTrack(source->Track()));
  auto push_frame_sync = [&](scoped_refptr<media::VideoFrame> video_frame) {
    base::RunLoop sink_loop;
    EXPECT_CALL(mock_sink, OnVideoFrame(_))
        .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
    platform_video_source->PushFrame(std::move(video_frame),
                                     base::TimeTicks::Now());
    sink_loop.Run();
  };

  Vector<scoped_refptr<media::VideoFrame>> frames;
  auto create_video_frame = [&]() {
    auto frame = media::VideoFrame::CreateBlackFrame(gfx::Size(10, 10));
    frames.push_back(frame);
    return frame;
  };
  auto get_frame_id = [&](int idx) { return frames[idx]->unique_id(); };

  EXPECT_TRUE(monitor.IsEmpty());
  // These frames are queued, pending to be read.
  for (size_t i = 0; i < max_frame_count; ++i) {
    auto video_frame = create_video_frame();
    media::VideoFrame::ID frame_id = video_frame->unique_id();
    push_frame_sync(std::move(video_frame));
    EXPECT_EQ(monitor.NumFrames(device_id), i + 1);
    EXPECT_EQ(monitor.NumRefs(device_id, frame_id), 1);
  }
  {
    // Push another video frame with the limit reached.
    auto video_frame = create_video_frame();
    media::VideoFrame::ID frame_id = video_frame->unique_id();
    push_frame_sync(std::move(video_frame));
    EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
    EXPECT_EQ(monitor.NumRefs(device_id, frame_id), 1);

    // The oldest frame should have been removed from the queue.
    EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(0)), 0);
  }

  auto* reader = stream->GetDefaultReaderForTesting(
      script_state, v8_scope.GetExceptionState());
  VideoFrame* video_frame1 = ReadObjectFromStream<VideoFrame>(v8_scope, reader);
  EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
  EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(1)), 1);

  VideoFrame* clone_frame1 = video_frame1->clone(v8_scope.GetExceptionState());
  EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
  EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(1)), 2);

  VideoFrame* video_frame2 = ReadObjectFromStream<VideoFrame>(v8_scope, reader);
  EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
  EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(2)), 1);

  // A new frame arrives, but the limit has been reached and there is nothing
  // that can be replaced.
  {
    auto video_frame = create_video_frame();
    media::VideoFrame::ID frame_id = video_frame->unique_id();
    push_frame_sync(std::move(video_frame));
    EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
    EXPECT_EQ(monitor.NumRefs(device_id, frame_id), 0);
  }

  // One of the JS VideoFrames backed by frames[1] is closed.
  clone_frame1->close();
  EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
  EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(1)), 1);

  // A new source connected to the same device is created and started in another
  // execution context.
  auto* media_stream_source2 = CreateDevicePushableSource(device_id);
  auto* platform_video_source2 =
      static_cast<blink::PushableMediaStreamVideoSource*>(
          media_stream_source2->GetPlatformSource());
  V8TestingScope v8_scope2;
  ScriptState* script_state2 = v8_scope2.GetScriptState();
  auto* track2 = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope2.GetExecutionContext(),
      MediaStreamVideoTrack::CreateVideoTrack(
          platform_video_source2,
          MediaStreamVideoSource::ConstraintsOnceCallback(),
          /*enabled=*/true));
  auto* source2 = CreateSource(
      script_state2, track2,
      MediaStreamVideoTrackUnderlyingSource::kMaxMonitoredFrameCount);
  ReadableStream::CreateWithCountQueueingStrategy(script_state2, source2, 0);

  MockMediaStreamVideoSink mock_sink2;
  mock_sink2.ConnectToTrack(WebMediaStreamTrack(source2->Track()));
  auto push_frame_sync2 = [&](scoped_refptr<media::VideoFrame> video_frame) {
    base::RunLoop sink_loop;
    EXPECT_CALL(mock_sink2, OnVideoFrame(_))
        .WillOnce(base::test::RunOnceClosure(sink_loop.QuitClosure()));
    platform_video_source2->PushFrame(std::move(video_frame),
                                      base::TimeTicks::Now());
    sink_loop.Run();
  };

  // The system delivers the last two created frames to the new source.
  {
    int idx = frames.size() - 2;
    EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
    EXPECT_GT(monitor.NumRefs(device_id, get_frame_id(idx)), 0);
    int num_refs = monitor.NumRefs(device_id, get_frame_id(idx));
    // The limit has been reached, but this frame is already monitored,
    // so it is queued.
    push_frame_sync2(frames[idx]);
    EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
    EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(idx)), num_refs + 1);
  }
  {
    int idx = frames.size() - 1;
    // The limit has been reached, and this frame was dropped by the other
    // source, so it is dropped by this one too.
    EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
    EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(idx)), 0);
    push_frame_sync2(frames[idx]);
    EXPECT_EQ(monitor.NumFrames(device_id), max_frame_count);
    EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(idx)), 0);
  }

  // The first context closes its source, but its VideoFrame objects are still
  // open.
  source->Close();

  // At this point, the only monitored frames are frames[1] and frames[2], both
  // open in context 1. frames[2] is also queued in context 2.
  EXPECT_EQ(monitor.NumFrames(device_id), 2u);

  // video_frame1 is the only reference to frames[1].
  EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(1)), 1);

  // video_frame2 is frames[2] and is open in context 1 and queued in context 2.
  EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(2)), 2);

  // Context 1 closes its video_frame1.
  video_frame1->close();
  EXPECT_EQ(monitor.NumFrames(device_id), 1u);
  EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(1)), 0);
  EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(2)), 2);

  // Context 1 closes its video_frame2, after which the only monitored frame is
  // the one queued by source 2.
  video_frame2->close();
  EXPECT_EQ(monitor.NumFrames(device_id), 1u);
  EXPECT_EQ(monitor.NumRefs(device_id, get_frame_id(2)), 1);

  // Context 2 closes its source, which should clear everything in the monitor.
  source2->Close();
  EXPECT_TRUE(monitor.IsEmpty());
}

TEST_F(MediaStreamVideoTrackUnderlyingSourceTest,
       VideoFramePrefersCaptureTimestamp) {
  const base::TimeDelta kTimestamp = base::Seconds(2);
  const base::TimeDelta kReferenceTimestamp = base::Seconds(3);
  const base::TimeDelta kCaptureTimestamp = base::Seconds(4);
  ASSERT_NE(kTimestamp, kCaptureTimestamp);

  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* track = CreateTrack(v8_scope.GetExecutionContext());
  auto* source = CreateSource(script_state, track);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  NonThrowableExceptionState exception_state;
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, exception_state);

  // Create and push a video frame with a capture and reference timestamp
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(10, 10));
  video_frame->set_timestamp(kTimestamp);
  media::VideoFrameMetadata metadata;
  metadata.capture_begin_time = base::TimeTicks() + kCaptureTimestamp;
  metadata.reference_time = base::TimeTicks() + kReferenceTimestamp;
  video_frame->set_metadata(metadata);

  PushableMediaStreamVideoSource* pushable_source =
      static_cast<PushableMediaStreamVideoSource*>(
          track->Component()->Source()->GetPlatformSource());
  pushable_source->PushFrame(std::move(video_frame), base::TimeTicks::Now());

  VideoFrame* web_video_frame =
      ReadObjectFromStream<VideoFrame>(v8_scope, reader);
  EXPECT_EQ(web_video_frame->timestamp(), kCaptureTimestamp.InMicroseconds());

  // Create and push a video frame with only a reference timestamp
  video_frame = media::VideoFrame::CreateBlackFrame(gfx::Size(10, 10));
  video_frame->set_timestamp(kTimestamp);
  metadata.capture_begin_time = std::nullopt;
  video_frame->set_metadata(metadata);
  pushable_source->PushFrame(std::move(video_frame), base::TimeTicks::Now());
  VideoFrame* web_video_frame2 =
      ReadObjectFromStream<VideoFrame>(v8_scope, reader);
  EXPECT_EQ(web_video_frame2->timestamp(),
            kReferenceTimestamp.InMicroseconds());

  // Create and push a new video frame without a capture or reference timestamp
  video_frame = media::VideoFrame::CreateBlackFrame(gfx::Size(10, 10));
  video_frame->set_timestamp(kTimestamp);
  EXPECT_FALSE(video_frame->metadata().capture_begin_time);
  EXPECT_FALSE(video_frame->metadata().reference_time);

  pushable_source->PushFrame(std::move(video_frame), base::TimeTicks::Now());
  VideoFrame* web_video_frame3 =
      ReadObjectFromStream<VideoFrame>(v8_scope, reader);

  if (base::FeatureList::IsEnabled(kBreakoutBoxInsertVideoCaptureTimestamp)) {
    scoped_refptr<media::VideoFrame> wrapped_video_frame3 =
        web_video_frame3->frame();
    ASSERT_TRUE(
        wrapped_video_frame3->metadata().capture_begin_time.has_value());
    EXPECT_EQ(web_video_frame3->timestamp(),
              (*wrapped_video_frame3->metadata().capture_begin_time -
               base::TimeTicks())
                  .InMicroseconds());
    ASSERT_TRUE(wrapped_video_frame3->metadata().reference_time.has_value());
    EXPECT_EQ(
        web_video_frame3->timestamp(),
        (*wrapped_video_frame3->metadata().reference_time - base::TimeTicks())
            .InMicroseconds());
  }

  source->Close();
  track->stopTrack(v8_scope.GetExecutionContext());
}

}  // namespace blink
