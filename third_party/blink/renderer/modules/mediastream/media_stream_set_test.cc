// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/media_stream_set.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/test/fake_image_capturer.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_request.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

using testing::_;

namespace blink {

namespace {

class MockLocalMediaStreamVideoSource : public blink::MediaStreamVideoSource {
 public:
  MockLocalMediaStreamVideoSource()
      : blink::MediaStreamVideoSource(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()) {}

 private:
  base::WeakPtr<MediaStreamVideoSource> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  void StartSourceImpl(
      VideoCaptureDeliverFrameCB frame_callback,
      EncodedVideoFrameCB encoded_frame_callback,
      VideoCaptureSubCaptureTargetVersionCB sub_capture_target_version_callback,
      VideoCaptureNotifyFrameDroppedCB frame_dropped_callback) override {}

  void StopSourceImpl() override {}

  base::WeakPtrFactory<MockLocalMediaStreamVideoSource> weak_factory_{this};
};

class MediaStreamSetTest : public testing::Test {
 public:
  MediaStreamSetTest() = default;
  ~MediaStreamSetTest() override { WebHeap::CollectAllGarbageForTesting(); }

 protected:
  // Required as persistent member to prevent the garbage collector from
  // removing the object before the test ended.
  test::TaskEnvironment task_environment_;
  Persistent<MediaStreamSet> media_stream_set_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

MediaStreamComponent* MakeMockVideoComponent() {
  auto platform_video_source =
      std::make_unique<MockLocalMediaStreamVideoSource>();
  auto* platform_video_source_ptr = platform_video_source.get();
  MediaStreamSource* const test_video_source =
      MakeGarbageCollected<MediaStreamSource>(
          /*id=*/"test_source_1_id", MediaStreamSource::StreamType::kTypeVideo,
          /*name=*/"test_source_1_name", /*remote=*/false,
          std::move(platform_video_source));

  return MakeGarbageCollected<MediaStreamComponentImpl>(
      test_video_source, std::make_unique<MediaStreamVideoTrack>(
                             platform_video_source_ptr,
                             MediaStreamVideoSource::ConstraintsOnceCallback(),
                             /*enabled=*/true));
}

// This test checks if |MediaStreamSet| calls the initialized callback if used
// for getAllScreensMedia with a single stream requested, i.e. one descriptor
// with one video source passed in the constructor.
TEST_F(MediaStreamSetTest, GetAllScreensMediaSingleMediaStreamInitialized) {
  V8TestingScope v8_scope;
  MediaStreamComponentVector audio_component_vector;
  MediaStreamComponentVector video_component_vector = {
      MakeMockVideoComponent()};
  MediaStreamDescriptor* const descriptor =
      MakeGarbageCollected<MediaStreamDescriptor>(audio_component_vector,
                                                  video_component_vector);
  MediaStreamDescriptorVector descriptors = {descriptor};
  base::RunLoop run_loop;
  media_stream_set_ = MakeGarbageCollected<MediaStreamSet>(
      v8_scope.GetExecutionContext(), descriptors,
      UserMediaRequestType::kAllScreensMedia,
      base::BindLambdaForTesting([&run_loop](MediaStreamVector streams) {
        EXPECT_EQ(streams.size(), 1u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test checks if |MediaStreamSet| calls the initialized callback if used
// for getAllScreensMedia with a multiple streams requested, i.e.
// multiple descriptors with one video source each passed in the constructor.
TEST_F(MediaStreamSetTest, GetAllScreensMediaMultipleMediaStreamsInitialized) {
  V8TestingScope v8_scope;
  MediaStreamComponentVector audio_component_vector;
  MediaStreamComponentVector video_component_vector = {
      MakeMockVideoComponent()};
  MediaStreamDescriptor* const descriptor =
      MakeGarbageCollected<MediaStreamDescriptor>(audio_component_vector,
                                                  video_component_vector);
  MediaStreamDescriptorVector descriptors = {descriptor, descriptor, descriptor,
                                             descriptor};
  base::RunLoop run_loop;
  media_stream_set_ = MakeGarbageCollected<MediaStreamSet>(
      v8_scope.GetExecutionContext(), descriptors,
      UserMediaRequestType::kAllScreensMedia,
      base::BindLambdaForTesting([&run_loop](MediaStreamVector streams) {
        EXPECT_EQ(streams.size(), 4u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test checks if |MediaStreamSet| calls the initialized callback if used
// for getAllScreensMedia with a no streams requested, i.e.
// an empty descriptors list.
TEST_F(MediaStreamSetTest, GetAllScreensMediaNoMediaStreamInitialized) {
  V8TestingScope v8_scope;
  MediaStreamDescriptorVector descriptors;
  base::RunLoop run_loop;
  media_stream_set_ = MakeGarbageCollected<MediaStreamSet>(
      v8_scope.GetExecutionContext(), descriptors,
      UserMediaRequestType::kAllScreensMedia,
      base::BindLambdaForTesting([&run_loop](MediaStreamVector streams) {
        EXPECT_TRUE(streams.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test checks if |MediaStreamSet| calls the initialized callback if used
// for getDisplayMedia with a single stream requested, i.e. one descriptor
// with one video source passed in the constructor.
TEST_F(MediaStreamSetTest, GetDisplayMediaSingleMediaStreamInitialized) {
  V8TestingScope v8_scope;

  // A fake image capturer is required for a video track to finish
  // initialization.
  FakeImageCapture fake_image_capturer;
  fake_image_capturer.RegisterBinding(v8_scope.GetExecutionContext());

  MediaStreamComponentVector audio_component_vector;
  MediaStreamComponentVector video_component_vector = {
      MakeMockVideoComponent()};
  MediaStreamDescriptor* const descriptor =
      MakeGarbageCollected<MediaStreamDescriptor>(audio_component_vector,
                                                  video_component_vector);
  MediaStreamDescriptorVector descriptors = {descriptor};
  base::RunLoop run_loop;
  media_stream_set_ = MakeGarbageCollected<MediaStreamSet>(
      v8_scope.GetExecutionContext(), descriptors,
      UserMediaRequestType::kDisplayMedia,
      base::BindLambdaForTesting([&run_loop](MediaStreamVector streams) {
        EXPECT_EQ(streams.size(), 1u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// This test checks if |MediaStreamSet| calls the initialized callback if used
// for getDisplayMedia with a no streams requested, i.e.
// an empty descriptors list.
TEST_F(MediaStreamSetTest, GetDisplayMediaNoMediaStreamInitialized) {
  V8TestingScope v8_scope;
  MediaStreamDescriptorVector descriptors;
  base::RunLoop run_loop;
  media_stream_set_ = MakeGarbageCollected<MediaStreamSet>(
      v8_scope.GetExecutionContext(), descriptors,
      UserMediaRequestType::kDisplayMedia,
      base::BindLambdaForTesting([&run_loop](MediaStreamVector streams) {
        EXPECT_TRUE(streams.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace

}  // namespace blink
