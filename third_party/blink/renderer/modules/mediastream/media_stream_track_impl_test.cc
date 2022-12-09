// Copyright 2021 The Chromium Authors
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
#include "third_party/blink/renderer/bindings/modules/v8/v8_constrain_long_range.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_constrainlongrange_long.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/modules/mediastream/local_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_sink.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
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

std::unique_ptr<MockMediaStreamVideoSource> MakeMockMediaStreamVideoSource() {
  return base::WrapUnique(new MockMediaStreamVideoSource(
      media::VideoCaptureFormat(gfx::Size(640, 480), 30.0,
                                media::PIXEL_FORMAT_I420),
      true));
}

std::unique_ptr<blink::LocalMediaStreamAudioSource>
MakeLocalMediaStreamAudioSource() {
  blink::MediaStreamDevice device;
  device.type = blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
  return std::make_unique<blink::LocalMediaStreamAudioSource>(
      /*blink::WebLocalFrame=*/nullptr, device,
      /*requested_buffer_size=*/nullptr,
      /*disable_local_echo=*/false,
      blink::WebPlatformMediaStreamSource::ConstraintsRepeatingCallback(),
      blink::scheduler::GetSingleThreadTaskRunnerForTesting());
}

}  // namespace

class MediaStreamTrackImplTest : public testing::Test {
 public:
  ~MediaStreamTrackImplTest() override {
    WebHeap::CollectAllGarbageForTesting();
  }

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(MediaStreamTrackImplTest, StopTrackTriggersObservers) {
  V8TestingScope v8_scope;

  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      false /* remote */, MakeMockMediaStreamVideoSource());
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source);
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  TestObserver* testObserver = MakeGarbageCollected<TestObserver>();
  track->AddObserver(testObserver);

  source->SetReadyState(MediaStreamSource::kReadyStateMuted);
  EXPECT_EQ(testObserver->ObservationCount(), 1);

  track->stopTrack(v8_scope.GetExecutionContext());
  EXPECT_EQ(testObserver->ObservationCount(), 2);
}

TEST_F(MediaStreamTrackImplTest, StopTrackSynchronouslyDisablesMedia) {
  V8TestingScope v8_scope;

  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeAudio, "name",
      false /* remote */, MakeMockMediaStreamVideoSource());
  auto platform_track =
      std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */);
  MediaStreamAudioTrack* platform_track_ptr = platform_track.get();
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source,
                                                     std::move(platform_track));
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  ASSERT_TRUE(platform_track_ptr->IsEnabled());
  track->stopTrack(v8_scope.GetExecutionContext());
  EXPECT_FALSE(platform_track_ptr->IsEnabled());
}

TEST_F(MediaStreamTrackImplTest, MutedStateUpdates) {
  V8TestingScope v8_scope;

  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      /*remote=*/false, /*platform_source=*/nullptr);
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source);
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  EXPECT_EQ(track->muted(), false);

  source->SetReadyState(MediaStreamSource::kReadyStateMuted);
  EXPECT_EQ(track->muted(), true);

  source->SetReadyState(MediaStreamSource::kReadyStateLive);
  EXPECT_EQ(track->muted(), false);
}

TEST_F(MediaStreamTrackImplTest, MutedDoesntUpdateAfterEnding) {
  V8TestingScope v8_scope;

  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      false /* remote */, MakeMockMediaStreamVideoSource());
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source);
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  ASSERT_EQ(track->muted(), false);

  track->stopTrack(v8_scope.GetExecutionContext());

  source->SetReadyState(MediaStreamSource::kReadyStateMuted);

  EXPECT_EQ(track->muted(), false);
}

TEST_F(MediaStreamTrackImplTest, CloneVideoTrack) {
  V8TestingScope v8_scope;

  std::unique_ptr<MediaStreamVideoSource> platform_source =
      MakeMockMediaStreamVideoSource();
  MediaStreamVideoSource* platform_source_ptr = platform_source.get();
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      false /* remote */, std::move(platform_source));
  auto platform_track = std::make_unique<MediaStreamVideoTrack>(
      platform_source_ptr,
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      true /* enabled */);
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source,
                                                     std::move(platform_track));
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  MediaStreamTrack* clone = track->clone(v8_scope.GetExecutionContext());

  // The clone should have a component initialized with a MediaStreamVideoTrack
  // instance as its platform track.
  EXPECT_TRUE(clone->Component()->GetPlatformTrack());
  EXPECT_TRUE(MediaStreamVideoTrack::From(clone->Component()));

  // Clones should share the same source object.
  EXPECT_EQ(clone->Component()->Source(), source);
}

TEST_F(MediaStreamTrackImplTest, CloneAudioTrack) {
  V8TestingScope v8_scope;

  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeAudio, "name",
      false /* remote */, MakeLocalMediaStreamAudioSource());
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source);
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  MediaStreamTrack* clone = track->clone(v8_scope.GetExecutionContext());

  // The clone should have a component initialized with a MediaStreamAudioTrack
  // instance as its platform track.
  EXPECT_TRUE(clone->Component()->GetPlatformTrack());
  EXPECT_TRUE(MediaStreamAudioTrack::From(clone->Component()));

  // Clones should share the same source object.
  EXPECT_EQ(clone->Component()->Source(), source);
}

TEST_F(MediaStreamTrackImplTest, CloningPreservesConstraints) {
  V8TestingScope v8_scope;

  auto platform_source = std::make_unique<MockMediaStreamVideoSource>(
      media::VideoCaptureFormat(gfx::Size(1280, 720), 1000.0,
                                media::PIXEL_FORMAT_I420),
      false);
  MockMediaStreamVideoSource* platform_source_ptr = platform_source.get();
  MediaStreamSource* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::StreamType::kTypeVideo, "name",
      false /* remote */, std::move(platform_source));
  auto platform_track = std::make_unique<MediaStreamVideoTrack>(
      platform_source_ptr,
      WebPlatformMediaStreamSource::ConstraintsOnceCallback(),
      true /* enabled */);
  MediaStreamComponent* component =
      MakeGarbageCollected<MediaStreamComponentImpl>(source,
                                                     std::move(platform_track));
  MediaStreamTrack* track = MakeGarbageCollected<MediaStreamTrackImpl>(
      v8_scope.GetExecutionContext(), component);

  MediaConstraints constraints;
  MediaTrackConstraintSetPlatform basic;
  basic.width.SetMax(240);
  constraints.Initialize(basic, Vector<MediaTrackConstraintSetPlatform>());
  track->SetInitialConstraints(constraints);

  MediaStreamTrack* clone = track->clone(v8_scope.GetExecutionContext());
  MediaTrackConstraints* clone_constraints = clone->getConstraints();
  EXPECT_TRUE(clone_constraints->hasWidth());
  EXPECT_EQ(clone_constraints->width()->GetAsConstrainLongRange()->max(), 240);
}

}  // namespace blink
