// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/video_track_signal_underlying_source.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_stream_track_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_generator.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_signal.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/stream_test_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

using testing::_;

namespace blink {

class VideoTrackSignalUnderlyingSourceTest : public testing::Test {
 public:
  ~VideoTrackSignalUnderlyingSourceTest() override {
    platform_->RunUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamTrackGenerator* CreateGenerator(ScriptState* script_state) {
    return MakeGarbageCollected<MediaStreamTrackGenerator>(
        script_state, MediaStreamSource::kTypeVideo, "test-generator");
  }

  VideoTrackSignalUnderlyingSource* CreateSource(
      ScriptState* script_state,
      MediaStreamTrackGenerator* generator,
      wtf_size_t max_buffer_size) {
    return MakeGarbageCollected<VideoTrackSignalUnderlyingSource>(
        script_state, generator, max_buffer_size);
  }

  VideoTrackSignalUnderlyingSource* CreateSource(
      ScriptState* script_state,
      MediaStreamTrackGenerator* generator) {
    return CreateSource(script_state, generator, 1u);
  }

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(VideoTrackSignalUnderlyingSourceTest, SignalsAreExposed) {
  V8TestingScope v8_scope;
  auto* script_state = v8_scope.GetScriptState();
  auto* generator = CreateGenerator(script_state);
  auto* video_track = MediaStreamVideoTrack::From(generator->Component());
  auto* video_source = generator->PushableVideoSource();
  auto* source = CreateSource(script_state, generator);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  video_source->RequestRefreshFrame();
  auto* signal = ReadObjectFromStream<MediaStreamTrackSignal>(v8_scope, reader);
  EXPECT_EQ(signal->signalType(), "request-frame");

  const double min_frame_rate = 3.5;
  video_track->SetMinimumFrameRate(min_frame_rate);
  signal = ReadObjectFromStream<MediaStreamTrackSignal>(v8_scope, reader);
  EXPECT_EQ(signal->signalType(), "set-min-frame-rate");
  EXPECT_TRUE(signal->hasFrameRate());
  EXPECT_EQ(signal->frameRate(), min_frame_rate);

  source->Close();
}

TEST_F(VideoTrackSignalUnderlyingSourceTest, CancelStreamDisconnectsFromTrack) {
  V8TestingScope v8_scope;
  auto* script_state = v8_scope.GetScriptState();
  auto* generator = CreateGenerator(script_state);
  auto* source = CreateSource(script_state, generator);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);
  auto& queue = source->QueueForTesting();

  generator->PushableVideoSource()->RequestRefreshFrame();
  EXPECT_EQ(queue.size(), 1u);

  source->Cancel(script_state, ScriptValue());
  EXPECT_EQ(queue.size(), 0u);

  generator->PushableVideoSource()->RequestRefreshFrame();
  EXPECT_EQ(queue.size(), 0u);

  source->Close();
}

TEST_F(VideoTrackSignalUnderlyingSourceTest, DropOldSignalsWhenQueueIsFull) {
  V8TestingScope v8_scope;
  auto* script_state = v8_scope.GetScriptState();
  auto* generator = CreateGenerator(script_state);
  auto* video_track = MediaStreamVideoTrack::From(generator->Component());
  const wtf_size_t buffer_size = 3;
  auto* source = CreateSource(script_state, generator, buffer_size);
  EXPECT_EQ(source->MaxQueueSize(), buffer_size);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  const auto& queue = source->QueueForTesting();
  for (wtf_size_t i = 0; i < buffer_size; ++i) {
    EXPECT_EQ(queue.size(), i);
    video_track->SetMinimumFrameRate(i);
    EXPECT_EQ(queue.back()->frameRate(), i);
    EXPECT_EQ(queue.front()->frameRate(), 0.0);
  }

  // Push another signal while the queue is full.
  EXPECT_EQ(queue.size(), buffer_size);
  video_track->SetMinimumFrameRate(buffer_size);

  // Since the queue was full, the oldest signal from the queue should have been
  // dropped.
  EXPECT_EQ(queue.size(), buffer_size);
  EXPECT_EQ(queue.back()->frameRate(), buffer_size);
  EXPECT_EQ(queue.front()->frameRate(), 1.0);

  // Pulling with signals in the queue should move the oldest signal in the
  // queue to the stream's controller.
  EXPECT_EQ(source->DesiredSizeForTesting(), 0);
  EXPECT_FALSE(source->IsPendingPullForTesting());
  source->pull(script_state);
  EXPECT_EQ(source->DesiredSizeForTesting(), -1);
  EXPECT_FALSE(source->IsPendingPullForTesting());
  EXPECT_EQ(queue.size(), buffer_size - 1);
  EXPECT_EQ(queue.front()->frameRate(), 2);

  source->Close();
  EXPECT_EQ(queue.size(), 0u);
}

TEST_F(VideoTrackSignalUnderlyingSourceTest,
       BypassQueueAfterPullWithEmptyBuffer) {
  V8TestingScope v8_scope;
  auto* script_state = v8_scope.GetScriptState();
  auto* generator = CreateGenerator(script_state);
  auto* source = CreateSource(script_state, generator);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  // At first, the queue is empty and the desired size is empty as well.
  EXPECT_TRUE(source->QueueForTesting().empty());
  EXPECT_EQ(source->DesiredSizeForTesting(), 0);
  EXPECT_FALSE(source->IsPendingPullForTesting());

  source->pull(script_state);
  EXPECT_TRUE(source->QueueForTesting().empty());
  EXPECT_EQ(source->DesiredSizeForTesting(), 0);
  EXPECT_TRUE(source->IsPendingPullForTesting());

  generator->PushableVideoSource()->RequestRefreshFrame();
  // Since a pull was pending, the signal is put directly in the stream
  // controller, bypassing the source queue.
  EXPECT_TRUE(source->QueueForTesting().empty());
  EXPECT_EQ(source->DesiredSizeForTesting(), -1);
  EXPECT_FALSE(source->IsPendingPullForTesting());

  source->Close();
}

TEST_F(VideoTrackSignalUnderlyingSourceTest, QueueSizeCannotBeZero) {
  V8TestingScope v8_scope;
  auto* script_state = v8_scope.GetScriptState();
  auto* generator = CreateGenerator(script_state);
  auto* source = CreateSource(script_state, generator, /*max_buffer_size=*/0u);

  // Queue size is always at least 1, even if 0 is requested.
  EXPECT_EQ(source->MaxQueueSize(), 1u);
  source->Close();
}

}  // namespace blink
