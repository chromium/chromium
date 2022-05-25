// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

using testing::_;

namespace blink {

namespace {

class MockLocalMediaStreamVideoSource : public blink::MediaStreamVideoSource {
 public:
  MockLocalMediaStreamVideoSource()
      : blink::MediaStreamVideoSource(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()) {}

 private:
  base::WeakPtr<MediaStreamVideoSource> GetWeakPtr() const override {
    return weak_factory_.GetWeakPtr();
  }

  void StartSourceImpl(VideoCaptureDeliverFrameCB frame_callback,
                       EncodedVideoFrameCB encoded_frame_callback) override {}

  void StopSourceImpl() override {}

  base::WeakPtrFactory<MockLocalMediaStreamVideoSource> weak_factory_{this};
};

class MediaStreamSetTest : public testing::Test {
 public:
  MediaStreamSetTest() = default;
  ~MediaStreamSetTest() override { WebHeap::CollectAllGarbageForTesting(); }
};

TEST_F(MediaStreamSetTest, SingleMediaStreamInitialized) {
  V8TestingScope v8_scope;
  Member<MediaStreamSource> test_video_source =
      MakeGarbageCollected<MediaStreamSource>(
          "test_source_1", MediaStreamSource::StreamType::kTypeVideo,
          "test_source_1", false,
          std::make_unique<MockLocalMediaStreamVideoSource>());
  MediaStreamSourceVector audio_source_vector = {};
  MediaStreamSourceVector video_source_vector = {test_video_source};
  Member<MediaStreamDescriptor> descriptor =
      MakeGarbageCollected<MediaStreamDescriptor>(audio_source_vector,
                                                  video_source_vector);
  MediaStreamDescriptorVector descriptors = {descriptor};
  base::RunLoop run_loop;
  Member<MediaStreamSet> media_stream_set =
      MakeGarbageCollected<MediaStreamSet>(
          v8_scope.GetExecutionContext(), descriptors,
          base::BindLambdaForTesting([&run_loop](MediaStreamVector streams) {
            ASSERT_EQ(streams.size(), 1u);
            run_loop.Quit();
          }));
  DCHECK(media_stream_set);
  run_loop.Run();
}

TEST_F(MediaStreamSetTest, MultipleMediaStreamsInitialized) {
  V8TestingScope v8_scope;
  Member<MediaStreamSource> test_video_source =
      MakeGarbageCollected<MediaStreamSource>(
          "test_source_1", MediaStreamSource::StreamType::kTypeVideo,
          "test_source_1", false,
          std::make_unique<MockLocalMediaStreamVideoSource>());
  MediaStreamSourceVector audio_source_vector = {};
  MediaStreamSourceVector video_source_vector = {test_video_source};
  Member<MediaStreamDescriptor> descriptor =
      MakeGarbageCollected<MediaStreamDescriptor>(audio_source_vector,
                                                  video_source_vector);
  MediaStreamDescriptorVector descriptors = {descriptor, descriptor, descriptor,
                                             descriptor};
  base::RunLoop run_loop;
  Member<MediaStreamSet> media_stream_set =
      MakeGarbageCollected<MediaStreamSet>(
          v8_scope.GetExecutionContext(), descriptors,
          base::BindLambdaForTesting([&run_loop](MediaStreamVector streams) {
            ASSERT_EQ(streams.size(), 4u);
            run_loop.Quit();
          }));
  DCHECK(media_stream_set);
  run_loop.Run();
}

TEST_F(MediaStreamSetTest, NoTracksInStream) {
  V8TestingScope v8_scope;
  MediaStreamSourceVector audio_source_vector = {};
  MediaStreamSourceVector video_source_vector = {};
  Member<MediaStreamDescriptor> descriptor =
      MakeGarbageCollected<MediaStreamDescriptor>(audio_source_vector,
                                                  video_source_vector);
  MediaStreamDescriptorVector descriptors = {descriptor};
  base::RunLoop run_loop;
  Member<MediaStreamSet> media_stream_set =
      MakeGarbageCollected<MediaStreamSet>(
          v8_scope.GetExecutionContext(), descriptors,
          base::BindLambdaForTesting([&run_loop](MediaStreamVector streams) {
            ASSERT_EQ(streams.size(), 1u);
            ASSERT_EQ(streams[0]->getVideoTracks().size(), 0u);
            ASSERT_EQ(streams[0]->getAudioTracks().size(), 0u);
            run_loop.Quit();
          }));
  DCHECK(media_stream_set);
  run_loop.Run();
}

TEST_F(MediaStreamSetTest, NoMediaStreamInitialized) {
  V8TestingScope v8_scope;
  MediaStreamSourceVector audio_source_vector = {};
  MediaStreamSourceVector video_source_vector = {};
  MediaStreamDescriptorVector descriptors;
  base::RunLoop run_loop;
  Member<MediaStreamSet> media_stream_set =
      MakeGarbageCollected<MediaStreamSet>(
          v8_scope.GetExecutionContext(), descriptors,
          base::BindLambdaForTesting([&run_loop](MediaStreamVector streams) {
            ASSERT_TRUE(streams.IsEmpty());
            run_loop.Quit();
          }));
  DCHECK(media_stream_set);
  run_loop.Run();
}

}  // namespace

}  // namespace blink
