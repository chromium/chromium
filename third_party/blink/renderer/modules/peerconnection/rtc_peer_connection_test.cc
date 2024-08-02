// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"

#include <string>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_answer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_ice_server.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_offer_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_peer_connection_error_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_session_description_init.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_track_impl.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_platform.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_track_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/rtc_error.h"
#include "v8/include/v8.h"

namespace blink {

class RTCOfferOptionsPlatform;

class RTCPeerConnectionTest : public testing::Test {
 public:
  RTCPeerConnection* CreatePC(V8TestingScope& scope) {
    RTCConfiguration* config = RTCConfiguration::Create();
    RTCIceServer* ice_server = RTCIceServer::Create();
    ice_server->setUrl("stun:fake.stun.url");
    HeapVector<Member<RTCIceServer>> ice_servers;
    ice_servers.push_back(ice_server);
    config->setIceServers(ice_servers);
    RTCPeerConnection::SetRtcPeerConnectionHandlerFactoryForTesting(
        base::BindRepeating(
            &RTCPeerConnectionTest::CreateRTCPeerConnectionHandler,
            base::Unretained(this)));
    return RTCPeerConnection::Create(scope.GetExecutionContext(), config,
                                     scope.GetExceptionState());
  }

  virtual std::unique_ptr<RTCPeerConnectionHandler>
  CreateRTCPeerConnectionHandler() {
    return std::make_unique<MockRTCPeerConnectionHandlerPlatform>();
  }

  MediaStreamTrack* CreateTrack(V8TestingScope& scope,
                                MediaStreamSource::StreamType type,
                                String id) {
    auto platform_source = std::make_unique<MockMediaStreamVideoSource>();
    auto* platform_source_ptr = platform_source.get();
    auto* source = MakeGarbageCollected<MediaStreamSource>(
        "sourceId", type, "sourceName", /*remote=*/false,
        std::move(platform_source));
    std::unique_ptr<MediaStreamTrackPlatform> platform_track;
    if (type == MediaStreamSource::kTypeAudio) {
      platform_track =
          std::make_unique<MediaStreamAudioTrack>(/*is_local_track=*/true);
    } else {
      platform_track = std::make_unique<MediaStreamVideoTrack>(
          platform_source_ptr,
          MediaStreamVideoSource::ConstraintsOnceCallback(),
          /*enabled=*/true);
    }
    auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
        id, source, std::move(platform_track));
    return MakeGarbageCollected<MediaStreamTrackImpl>(
        scope.GetExecutionContext(), component);
  }

  std::string GetExceptionMessage(V8TestingScope& scope) {
    ExceptionState& exception_state = scope.GetExceptionState();
    return exception_state.HadException() ? exception_state.Message().Utf8()
                                          : "";
  }

  void AddStream(V8TestingScope& scope,
                 RTCPeerConnection* pc,
                 MediaStream* stream) {
    pc->addStream(scope.GetScriptState(), stream, scope.GetExceptionState());
    EXPECT_EQ("", GetExceptionMessage(scope));
  }

  void RemoveStream(V8TestingScope& scope,
                    RTCPeerConnection* pc,
                    MediaStream* stream) {
    pc->removeStream(stream, scope.GetExceptionState());
    EXPECT_EQ("", GetExceptionMessage(scope));
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
};

TEST_F(RTCPeerConnectionTest, GetAudioTrack) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(pc->GetTrackForTesting(track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(pc->GetTrackForTesting(track->Component()));
}

TEST_F(RTCPeerConnectionTest, GetVideoTrack) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeVideo, "videoTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(pc->GetTrackForTesting(track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(pc->GetTrackForTesting(track->Component()));
}

TEST_F(RTCPeerConnectionTest, GetAudioAndVideoTrack) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  HeapVector<Member<MediaStreamTrack>> tracks;
  MediaStreamTrack* audio_track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  tracks.push_back(audio_track);
  MediaStreamTrack* video_track =
      CreateTrack(scope, MediaStreamSource::kTypeVideo, "videoTrack");
  tracks.push_back(video_track);

  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(pc->GetTrackForTesting(audio_track->Component()));
  EXPECT_FALSE(pc->GetTrackForTesting(video_track->Component()));
  AddStream(scope, pc, stream);
  EXPECT_TRUE(pc->GetTrackForTesting(audio_track->Component()));
  EXPECT_TRUE(pc->GetTrackForTesting(video_track->Component()));
}

TEST_F(RTCPeerConnectionTest, GetTrackRemoveStreamAndGCAll) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  MediaStreamComponent* track_component = track->Component();

  {
    HeapVector<Member<MediaStreamTrack>> tracks;
    tracks.push_back(track);
    MediaStream* stream =
        MediaStream::Create(scope.GetExecutionContext(), tracks);
    ASSERT_TRUE(stream);

    EXPECT_FALSE(pc->GetTrackForTesting(track_component));
    AddStream(scope, pc, stream);
    EXPECT_TRUE(pc->GetTrackForTesting(track_component));

    RemoveStream(scope, pc, stream);
    // Transceivers will still reference the stream even after it is "removed".
    // To make the GC tests work, clear the stream from tracks so that the
    // stream does not keep tracks alive.
    while (!stream->getTracks().empty())
      stream->removeTrack(stream->getTracks()[0], scope.GetExceptionState());
  }

  // This will destroy |MediaStream|, |MediaStreamTrack| and its
  // |MediaStreamComponent|, which will remove its mapping from the peer
  // connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(pc->GetTrackForTesting(track_component));
}

TEST_F(RTCPeerConnectionTest,
       GetTrackRemoveStreamAndGCWithPersistentComponent) {
  V8TestingScope scope;
  Persistent<RTCPeerConnection> pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);

  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  Persistent<MediaStreamComponent> track_component = track->Component();

  {
    HeapVector<Member<MediaStreamTrack>> tracks;
    tracks.push_back(track);
    MediaStream* stream =
        MediaStream::Create(scope.GetExecutionContext(), tracks);
    ASSERT_TRUE(stream);

    EXPECT_FALSE(pc->GetTrackForTesting(track_component.Get()));
    AddStream(scope, pc, stream);
    EXPECT_TRUE(pc->GetTrackForTesting(track_component.Get()));

    RemoveStream(scope, pc, stream);
    // Transceivers will still reference the stream even after it is "removed".
    // To make the GC tests work, clear the stream from tracks so that the
    // stream does not keep tracks alive.
    while (!stream->getTracks().empty())
      stream->removeTrack(stream->getTracks()[0], scope.GetExceptionState());
  }

  // This will destroy |MediaStream| and |MediaStreamTrack| (but not
  // |MediaStreamComponent|), which will remove its mapping from the peer
  // connection.
  WebHeap::CollectAllGarbageForTesting();
  EXPECT_FALSE(pc->GetTrackForTesting(track_component.Get()));
}

enum class AsyncOperationAction {
  kLeavePending,
  kResolve,
  kReject,
};

template <typename RequestType>
void CompleteRequest(RequestType* request, bool resolve);

template <>
void CompleteRequest(RTCVoidRequest* request, bool resolve) {
  if (resolve) {
    request->RequestSucceeded();
  } else {
    request->RequestFailed(
        webrtc::RTCError(webrtc::RTCErrorType::INVALID_MODIFICATION));
  }
}

template <>
void CompleteRequest(RTCSessionDescriptionRequest* request, bool resolve) {
  if (resolve) {
    auto* description =
        MakeGarbageCollected<RTCSessionDescriptionPlatform>(String(), String());
    request->RequestSucceeded(description);
  } else {
    request->RequestFailed(
        webrtc::RTCError(webrtc::RTCErrorType::INVALID_MODIFICATION));
  }
}

template <typename RequestType>
void PostToCompleteRequest(AsyncOperationAction action, RequestType* request) {
  switch (action) {
    case AsyncOperationAction::kLeavePending:
      return;
    case AsyncOperationAction::kResolve:
      scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
          FROM_HERE, WTF::BindOnce(&CompleteRequest<RequestType>,
                                   WrapWeakPersistent(request), true));
      return;
    case AsyncOperationAction::kReject:
      scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
          FROM_HERE, WTF::BindOnce(&CompleteRequest<RequestType>,
                                   WrapWeakPersistent(request), false));
      return;
  }
}

class FakeRTCPeerConnectionHandlerPlatform
    : public MockRTCPeerConnectionHandlerPlatform {
 public:
  Vector<std::unique_ptr<RTCRtpTransceiverPlatform>> CreateOffer(
      RTCSessionDescriptionRequest* request,
      RTCOfferOptionsPlatform*) override {
    PostToCompleteRequest<RTCSessionDescriptionRequest>(async_operation_action_,
                                                        request);
    return {};
  }

  void CreateAnswer(RTCSessionDescriptionRequest* request,
                    RTCAnswerOptionsPlatform*) override {
    PostToCompleteRequest<RTCSessionDescriptionRequest>(async_operation_action_,
                                                        request);
  }

  void SetLocalDescription(RTCVoidRequest* request,
                           ParsedSessionDescription) override {
    PostToCompleteRequest<RTCVoidRequest>(async_operation_action_, request);
  }

  void SetRemoteDescription(RTCVoidRequest* request,
                            ParsedSessionDescription) override {
    PostToCompleteRequest<RTCVoidRequest>(async_operation_action_, request);
  }

  void set_async_operation_action(AsyncOperationAction action) {
    async_operation_action_ = action;
  }

 private:
  // Decides what to do with future async operations' promises/callbacks.
  AsyncOperationAction async_operation_action_ =
      AsyncOperationAction::kLeavePending;
};

TEST_F(RTCPeerConnectionTest, MediaStreamTrackStopsThrottling) {
  V8TestingScope scope;

  auto* scheduler = scope.GetFrame().GetFrameScheduler()->GetPageScheduler();
  EXPECT_FALSE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // Creating the RTCPeerConnection doesn't disable throttling.
  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_EQ("", GetExceptionMessage(scope));
  ASSERT_TRUE(pc);
  EXPECT_FALSE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // But creating a media stream track does.
  MediaStreamTrack* track =
      CreateTrack(scope, MediaStreamSource::kTypeAudio, "audioTrack");
  HeapVector<Member<MediaStreamTrack>> tracks;
  tracks.push_back(track);
  MediaStream* stream =
      MediaStream::Create(scope.GetExecutionContext(), tracks);
  ASSERT_TRUE(stream);
  EXPECT_TRUE(scheduler->OptedOutFromAggressiveThrottlingForTest());

  // Stopping the track disables the opt-out.
  track->stopTrack(scope.GetExecutionContext());
  EXPECT_FALSE(scheduler->OptedOutFromAggressiveThrottlingForTest());
}

TEST_F(RTCPeerConnectionTest, GettingRtpTransportEarlySucceeds) {
  V8TestingScope scope;

  RTCPeerConnection* pc = CreatePC(scope);
  EXPECT_NE(pc->rtpTransport(), nullptr);
  EXPECT_EQ("", GetExceptionMessage(scope));
}

}  // namespace blink
