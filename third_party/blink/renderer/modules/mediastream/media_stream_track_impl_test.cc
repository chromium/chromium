// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"

#include <iostream>
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

using testing::_;

namespace blink {

namespace {

class TestObserver : public GarbageCollected<TestObserver>,
                     public MediaStreamTrack::Observer {
 public:
  void TrackChangedState() override { observation_count_++; }
  int ObservationCount() const { return observation_count_; }

 private:
  int observation_count_ = 0;
};

}  // namespace

class MediaStreamTrackImplTest : public testing::Test {
 public:
  ~MediaStreamTrackImplTest() override {
    WebHeap::CollectAllGarbageForTesting();
  }
};

TEST_F(MediaStreamTrackImplTest, StopTrackTriggersObservers) {
  V8TestingScope v8_scope;

  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      false /* remote */);
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponent>(source);
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  TestObserver* testObserver = MakeGarbageCollected<TestObserver>();
  track->AddObserver(testObserver);

  source->SetReadyState(MediaStreamSource::kReadyStateMuted);
  EXPECT_EQ(testObserver->ObservationCount(), 1);

  track->stopTrack(v8_scope.GetExecutionContext());
  EXPECT_EQ(testObserver->ObservationCount(), 2);
}

TEST_F(MediaStreamTrackImplTest, LabelSanitizer) {
  V8TestingScope v8_scope;

  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeAudio, "Chromiums AirPods",
      false /* remote */);
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponent>(source);
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);
  EXPECT_EQ(track->label(), "AirPods");
}

}  // namespace blink
