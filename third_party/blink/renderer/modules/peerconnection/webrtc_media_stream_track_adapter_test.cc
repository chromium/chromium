// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class WebRtcMediaStreamTrackAdapterTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_ =
        MakeGarbageCollected<MockPeerConnectionDependencyFactory>();
    main_thread_ = blink::scheduler::GetSingleThreadTaskRunnerForTesting();
  }

  void TearDown() override {
    if (track_adapter_) {
      EXPECT_TRUE(track_adapter_->is_initialized());
      track_adapter_->Dispose();
      track_adapter_ = nullptr;
      RunMessageLoopsUntilIdle();
    }
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamComponent* CreateLocalAudioTrack() {
    auto audio_source = std::make_unique<MediaStreamAudioSource>(
        scheduler::GetSingleThreadTaskRunnerForTesting(), true);
    auto* audio_source_ptr = audio_source.get();
    auto* source = MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8("local_audio_id"), MediaStreamSource::kTypeAudio,
        String::FromUTF8("local_audio_track"), false, std::move(audio_source));

    auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
        source->Id(), source,
        std::make_unique<MediaStreamAudioTrack>(/*is_local=*/true));
    audio_source_ptr->ConnectToInitializedTrack(component);
    return component;
  }

  MediaStreamComponent* CreateLocalVideoTrack() {
    auto video_source = std::make_unique<MockMediaStreamVideoSource>();
    auto* video_source_ptr = video_source.get();
    // Dropping the MediaStreamSource reference here is ok, as video_source will
    // have a weak pointer to it as Owner(), which is picked up by the
    // MediaStreamComponent created with CreateVideoTrack() below.
    // TODO(https://crbug.com/1302689): Fix this crazy lifecycle jumping back
    // and forth between GCed and non-GCed objects...
    MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8("local_video_id"), MediaStreamSource::kTypeVideo,
        String::FromUTF8("local_video_track"), false, std::move(video_source));

    return MediaStreamVideoTrack::CreateVideoTrack(
        video_source_ptr,
        blink::MediaStreamVideoSource::ConstraintsOnceCallback(), true);
  }

  void CreateRemoteTrackAdapter(
      webrtc::MediaStreamTrackInterface* webrtc_track) {
    track_adapter_ =
        blink::WebRtcMediaStreamTrackAdapter::CreateRemoteTrackAdapter(
            dependency_factory_.Get(), main_thread_, webrtc_track);
  }

  void HoldOntoAdapterReference(
      base::WaitableEvent* waitable_event,
      scoped_refptr<blink::WebRtcMediaStreamTrackAdapter> adapter) {
    waitable_event->Wait();
  }

  // Runs message loops on the webrtc signaling thread and optionally the main
  // thread until idle.
  void RunMessageLoopsUntilIdle(bool run_loop_on_main_thread = true) {
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    dependency_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&WebRtcMediaStreamTrackAdapterTest::
                                      RunMessageLoopUntilIdleOnSignalingThread,
                                  base::Unretained(this), &waitable_event));
    waitable_event.Wait();
    if (run_loop_on_main_thread)
      base::RunLoop().RunUntilIdle();
  }

  void RunMessageLoopUntilIdleOnSignalingThread(
      base::WaitableEvent* waitable_event) {
    DCHECK(dependency_factory_->GetWebRtcSignalingTaskRunner()
               ->BelongsToCurrentThread());
    base::RunLoop().RunUntilIdle();
    waitable_event->Signal();
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  CrossThreadPersistent<MockPeerConnectionDependencyFactory>
      dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapter> track_adapter_;
};

TEST_F(WebRtcMediaStreamTrackAdapterTest, LocalAudioTrack) {
  track_adapter_ =
      blink::WebRtcMediaStreamTrackAdapter::CreateLocalTrackAdapter(
          dependency_factory_.Get(), main_thread_, CreateLocalAudioTrack());
  EXPECT_TRUE(track_adapter_->is_initialized());
  EXPECT_TRUE(track_adapter_->track());
  EXPECT_EQ(track_adapter_->track()->GetSourceType(),
            MediaStreamSource::kTypeAudio);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kAudioKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->track()->Id());
  EXPECT_TRUE(track_adapter_->GetLocalTrackAudioSinkForTesting());
  EXPECT_EQ(
      track_adapter_->GetLocalTrackAudioSinkForTesting()->webrtc_audio_track(),
      track_adapter_->webrtc_track());
}

// Flaky, see https://crbug.com/982200.
TEST_F(WebRtcMediaStreamTrackAdapterTest, DISABLED_LocalVideoTrack) {
  track_adapter_ =
      blink::WebRtcMediaStreamTrackAdapter::CreateLocalTrackAdapter(
          dependency_factory_.Get(), main_thread_, CreateLocalVideoTrack());
  EXPECT_TRUE(track_adapter_->is_initialized());
  EXPECT_TRUE(track_adapter_->track());
  EXPECT_EQ(track_adapter_->track()->GetSourceType(),
            MediaStreamSource::kTypeVideo);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kVideoKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->track()->Id());
  EXPECT_TRUE(track_adapter_->GetLocalTrackVideoSinkForTesting());
  EXPECT_EQ(
      track_adapter_->GetLocalTrackVideoSinkForTesting()->webrtc_video_track(),
      track_adapter_->webrtc_track());
}

TEST_F(WebRtcMediaStreamTrackAdapterTest, RemoteAudioTrack) {
  scoped_refptr<blink::MockWebRtcAudioTrack> webrtc_track =
      blink::MockWebRtcAudioTrack::Create("remote_audio_track");
  dependency_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcMediaStreamTrackAdapterTest::CreateRemoteTrackAdapter,
          base::Unretained(this), base::Unretained(webrtc_track.get())));
  // The adapter is initialized implicitly in a PostTask, allow it to run.
  RunMessageLoopsUntilIdle();
  DCHECK(track_adapter_);
  EXPECT_TRUE(track_adapter_->is_initialized());
  EXPECT_TRUE(track_adapter_->track());
  EXPECT_EQ(track_adapter_->track()->GetSourceType(),
            MediaStreamSource::kTypeAudio);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kAudioKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->track()->Id());
  EXPECT_TRUE(track_adapter_->GetRemoteAudioTrackAdapterForTesting());
  EXPECT_TRUE(
      track_adapter_->GetRemoteAudioTrackAdapterForTesting()->initialized());
}

TEST_F(WebRtcMediaStreamTrackAdapterTest, RemoteVideoTrack) {
  scoped_refptr<blink::MockWebRtcVideoTrack> webrtc_track =
      blink::MockWebRtcVideoTrack::Create("remote_video_track");
  dependency_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcMediaStreamTrackAdapterTest::CreateRemoteTrackAdapter,
          base::Unretained(this), base::Unretained(webrtc_track.get())));
  // The adapter is initialized implicitly in a PostTask, allow it to run.
  RunMessageLoopsUntilIdle();
  DCHECK(track_adapter_);
  EXPECT_TRUE(track_adapter_->is_initialized());
  EXPECT_TRUE(track_adapter_->track());
  EXPECT_EQ(track_adapter_->track()->GetSourceType(),
            MediaStreamSource::kTypeVideo);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kVideoKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->track()->Id());
  EXPECT_TRUE(track_adapter_->GetRemoteVideoTrackAdapterForTesting());
  EXPECT_TRUE(
      track_adapter_->GetRemoteVideoTrackAdapterForTesting()->initialized());
}

TEST_F(WebRtcMediaStreamTrackAdapterTest, RemoteTrackExplicitlyInitialized) {
  scoped_refptr<blink::MockWebRtcAudioTrack> webrtc_track =
      blink::MockWebRtcAudioTrack::Create("remote_audio_track");
  dependency_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcMediaStreamTrackAdapterTest::CreateRemoteTrackAdapter,
          base::Unretained(this), base::Unretained(webrtc_track.get())));
  // Wait for the CreateRemoteTrackAdapter() call, but don't run the main thread
  // loop that would have implicitly initialized the adapter.
  RunMessageLoopsUntilIdle(false);
  DCHECK(track_adapter_);
  EXPECT_FALSE(track_adapter_->is_initialized());
  // Explicitly initialize before the main thread loop has a chance to run.
  track_adapter_->InitializeOnMainThread();
  EXPECT_TRUE(track_adapter_->is_initialized());
  EXPECT_TRUE(track_adapter_->track());
  EXPECT_EQ(track_adapter_->track()->GetSourceType(),
            MediaStreamSource::kTypeAudio);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kAudioKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->track()->Id());
  EXPECT_TRUE(track_adapter_->GetRemoteAudioTrackAdapterForTesting());
  EXPECT_TRUE(
      track_adapter_->GetRemoteAudioTrackAdapterForTesting()->initialized());
}

TEST_F(WebRtcMediaStreamTrackAdapterTest, LastReferenceOnSignalingThread) {
  scoped_refptr<blink::MockWebRtcAudioTrack> webrtc_track =
      blink::MockWebRtcAudioTrack::Create("remote_audio_track");
  dependency_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcMediaStreamTrackAdapterTest::CreateRemoteTrackAdapter,
          base::Unretained(this), base::Unretained(webrtc_track.get())));
  // The adapter is initialized implicitly in a PostTask, allow it to run.
  RunMessageLoopsUntilIdle();
  DCHECK(track_adapter_);
  EXPECT_TRUE(track_adapter_->is_initialized());

  base::WaitableEvent waitable_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  dependency_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcMediaStreamTrackAdapterTest::HoldOntoAdapterReference,
          base::Unretained(this), base::Unretained(&waitable_event),
          track_adapter_));
  // Clear last reference on main thread.
  track_adapter_->Dispose();
  track_adapter_ = nullptr;
  waitable_event.Signal();
  RunMessageLoopsUntilIdle();
}

}  // namespace blink
