// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_task_environment.h"
#include "content/child/child_process.h"
#include "content/renderer/media/stream/media_stream_audio_source.h"
#include "content/renderer/media/stream/media_stream_video_track.h"
#include "content/renderer/media/stream/mock_media_stream_video_sink.h"
#include "content/renderer/media/stream/mock_media_stream_video_source.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_heap.h"

namespace content {

class WebRtcMediaStreamTrackAdapterTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
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

  blink::WebMediaStreamTrack CreateLocalAudioTrack() {
    blink::WebMediaStreamSource web_source;
    web_source.Initialize(blink::WebString::FromUTF8("local_audio_id"),
                          blink::WebMediaStreamSource::kTypeAudio,
                          blink::WebString::FromUTF8("local_audio_track"),
                          false);
    MediaStreamAudioSource* audio_source = new MediaStreamAudioSource(true);
    // Takes ownership of |audio_source|.
    web_source.SetExtraData(audio_source);

    blink::WebMediaStreamTrack web_track;
    web_track.Initialize(web_source.Id(), web_source);
    audio_source->ConnectToTrack(web_track);
    return web_track;
  }

  blink::WebMediaStreamTrack CreateLocalVideoTrack() {
    blink::WebMediaStreamSource web_source;
    web_source.Initialize(blink::WebString::FromUTF8("local_video_id"),
                          blink::WebMediaStreamSource::kTypeVideo,
                          blink::WebString::FromUTF8("local_video_track"),
                          false);
    MockMediaStreamVideoSource* video_source = new MockMediaStreamVideoSource();
    // Takes ownership of |video_source|.
    web_source.SetExtraData(video_source);

    return MediaStreamVideoTrack::CreateVideoTrack(
        video_source, MediaStreamVideoSource::ConstraintsCallback(), true);
  }

  void CreateRemoteTrackAdapter(
      webrtc::MediaStreamTrackInterface* webrtc_track) {
    track_adapter_ = WebRtcMediaStreamTrackAdapter::CreateRemoteTrackAdapter(
        dependency_factory_.get(), main_thread_, webrtc_track);
  }

  void HoldOntoAdapterReference(
      base::WaitableEvent* waitable_event,
      scoped_refptr<WebRtcMediaStreamTrackAdapter> adapter) {
    waitable_event->Wait();
  }

  // Runs message loops on the webrtc signaling thread and optionally the main
  // thread until idle.
  void RunMessageLoopsUntilIdle(bool run_loop_on_main_thread = true) {
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    dependency_factory_->GetWebRtcSignalingThread()->PostTask(
        FROM_HERE, base::BindOnce(&WebRtcMediaStreamTrackAdapterTest::
                                      RunMessageLoopUntilIdleOnSignalingThread,
                                  base::Unretained(this), &waitable_event));
    waitable_event.Wait();
    if (run_loop_on_main_thread)
      base::RunLoop().RunUntilIdle();
  }

  void RunMessageLoopUntilIdleOnSignalingThread(
      base::WaitableEvent* waitable_event) {
    DCHECK(dependency_factory_->GetWebRtcSignalingThread()
               ->BelongsToCurrentThread());
    base::RunLoop().RunUntilIdle();
    waitable_event->Signal();
  }

 protected:
  // The ScopedTaskEnvironment prevents the ChildProcess from leaking a
  // TaskScheduler.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ChildProcess child_process_;

  std::unique_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<WebRtcMediaStreamTrackAdapter> track_adapter_;
};

TEST_F(WebRtcMediaStreamTrackAdapterTest, LocalAudioTrack) {
  track_adapter_ = WebRtcMediaStreamTrackAdapter::CreateLocalTrackAdapter(
      dependency_factory_.get(), main_thread_, CreateLocalAudioTrack());
  EXPECT_TRUE(track_adapter_->is_initialized());
  EXPECT_TRUE(!track_adapter_->web_track().IsNull());
  EXPECT_EQ(track_adapter_->web_track().Source().GetType(),
            blink::WebMediaStreamSource::kTypeAudio);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kAudioKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->web_track().Id());
  EXPECT_TRUE(track_adapter_->GetLocalTrackAudioSinkForTesting());
  EXPECT_EQ(
      track_adapter_->GetLocalTrackAudioSinkForTesting()->webrtc_audio_track(),
      track_adapter_->webrtc_track());
}

TEST_F(WebRtcMediaStreamTrackAdapterTest, LocalVideoTrack) {
  track_adapter_ = WebRtcMediaStreamTrackAdapter::CreateLocalTrackAdapter(
      dependency_factory_.get(), main_thread_, CreateLocalVideoTrack());
  EXPECT_TRUE(track_adapter_->is_initialized());
  EXPECT_TRUE(!track_adapter_->web_track().IsNull());
  EXPECT_EQ(track_adapter_->web_track().Source().GetType(),
            blink::WebMediaStreamSource::kTypeVideo);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kVideoKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->web_track().Id());
  EXPECT_TRUE(track_adapter_->GetLocalTrackVideoSinkForTesting());
  EXPECT_EQ(
      track_adapter_->GetLocalTrackVideoSinkForTesting()->webrtc_video_track(),
      track_adapter_->webrtc_track());
}

TEST_F(WebRtcMediaStreamTrackAdapterTest, RemoteAudioTrack) {
  scoped_refptr<MockWebRtcAudioTrack> webrtc_track =
      MockWebRtcAudioTrack::Create("remote_audio_track");
  dependency_factory_->GetWebRtcSignalingThread()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcMediaStreamTrackAdapterTest::CreateRemoteTrackAdapter,
          base::Unretained(this), base::Unretained(webrtc_track.get())));
  // The adapter is initialized implicitly in a PostTask, allow it to run.
  RunMessageLoopsUntilIdle();
  DCHECK(track_adapter_);
  EXPECT_TRUE(track_adapter_->is_initialized());
  EXPECT_TRUE(!track_adapter_->web_track().IsNull());
  EXPECT_EQ(track_adapter_->web_track().Source().GetType(),
            blink::WebMediaStreamSource::kTypeAudio);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kAudioKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->web_track().Id());
  EXPECT_TRUE(track_adapter_->GetRemoteAudioTrackAdapterForTesting());
  EXPECT_TRUE(
      track_adapter_->GetRemoteAudioTrackAdapterForTesting()->initialized());
}

TEST_F(WebRtcMediaStreamTrackAdapterTest, RemoteVideoTrack) {
  scoped_refptr<MockWebRtcVideoTrack> webrtc_track =
      MockWebRtcVideoTrack::Create("remote_video_track");
  dependency_factory_->GetWebRtcSignalingThread()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebRtcMediaStreamTrackAdapterTest::CreateRemoteTrackAdapter,
          base::Unretained(this), base::Unretained(webrtc_track.get())));
  // The adapter is initialized implicitly in a PostTask, allow it to run.
  RunMessageLoopsUntilIdle();
  DCHECK(track_adapter_);
  EXPECT_TRUE(track_adapter_->is_initialized());
  EXPECT_TRUE(!track_adapter_->web_track().IsNull());
  EXPECT_EQ(track_adapter_->web_track().Source().GetType(),
            blink::WebMediaStreamSource::kTypeVideo);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kVideoKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->web_track().Id());
  EXPECT_TRUE(track_adapter_->GetRemoteVideoTrackAdapterForTesting());
  EXPECT_TRUE(
      track_adapter_->GetRemoteVideoTrackAdapterForTesting()->initialized());
}

TEST_F(WebRtcMediaStreamTrackAdapterTest, RemoteTrackExplicitlyInitialized) {
  scoped_refptr<MockWebRtcAudioTrack> webrtc_track =
      MockWebRtcAudioTrack::Create("remote_audio_track");
  dependency_factory_->GetWebRtcSignalingThread()->PostTask(
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
  EXPECT_TRUE(!track_adapter_->web_track().IsNull());
  EXPECT_EQ(track_adapter_->web_track().Source().GetType(),
            blink::WebMediaStreamSource::kTypeAudio);
  EXPECT_TRUE(track_adapter_->webrtc_track());
  EXPECT_EQ(track_adapter_->webrtc_track()->kind(),
            webrtc::MediaStreamTrackInterface::kAudioKind);
  EXPECT_EQ(track_adapter_->webrtc_track()->id().c_str(),
            track_adapter_->web_track().Id());
  EXPECT_TRUE(track_adapter_->GetRemoteAudioTrackAdapterForTesting());
  EXPECT_TRUE(
      track_adapter_->GetRemoteAudioTrackAdapterForTesting()->initialized());
}

TEST_F(WebRtcMediaStreamTrackAdapterTest, LastReferenceOnSignalingThread) {
  scoped_refptr<MockWebRtcAudioTrack> webrtc_track =
      MockWebRtcAudioTrack::Create("remote_audio_track");
  dependency_factory_->GetWebRtcSignalingThread()->PostTask(
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
  dependency_factory_->GetWebRtcSignalingThread()->PostTask(
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

}  // namespace content
