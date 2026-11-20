// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/test_webrtc_stats_report_obtainer.h"
#include "third_party/blink/renderer/modules/peerconnection/testing/mock_rtp_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_sender_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/make_ref_counted.h"
#include "third_party/webrtc/api/rtc_error.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/sframe/sframe_encryptor_interface.h"
#include "third_party/webrtc/api/sframe/sframe_types.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"
#include "third_party/webrtc/api/units/timestamp.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using base::test::ScopedFeatureList;
using ::testing::_;
using ::testing::Return;

namespace blink {

class RTCRtpSenderImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_ =
        MakeGarbageCollected<MockPeerConnectionDependencyFactory>();
    main_thread_ = blink::scheduler::GetSingleThreadTaskRunnerForTesting();
    track_map_ = base::MakeRefCounted<blink::WebRtcMediaStreamTrackAdapterMap>(
        dependency_factory_.Get(), main_thread_);
    peer_connection_ =
        new webrtc::RefCountedObject<blink::MockPeerConnectionImpl>(
            dependency_factory_.Get(), nullptr);
    mock_webrtc_sender_ = new webrtc::RefCountedObject<MockRtpSender>();
  }

  void TearDown() override {
    sender_.reset();
    // Syncing up with the signaling thread ensures any pending operations on
    // that thread are executed. If they post back to the main thread, such as
    // the sender's destructor traits, this is allowed to execute before the
    // test shuts down the threads.
    SyncWithSignalingThread();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  // Wait for the signaling thread to perform any queued tasks, executing tasks
  // posted to the current thread in the meantime while waiting.
  void SyncWithSignalingThread() const {
    base::RunLoop run_loop;
    dependency_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  MediaStreamComponent* CreateTrack(const std::string& id) {
    auto audio_source = std::make_unique<MediaStreamAudioSource>(
        blink::scheduler::GetSingleThreadTaskRunnerForTesting(), true);
    auto* audio_source_ptr = audio_source.get();
    auto* source = MakeGarbageCollected<MediaStreamSource>(
        String::FromUtf8(id), MediaStreamSource::kTypeAudio,
        "local_audio_track", false, std::move(audio_source));

    auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
        source->Id(), source,
        std::make_unique<MediaStreamAudioTrack>(/*is_local=*/true));
    audio_source_ptr->ConnectToInitializedTrack(component);
    return component;
  }

  std::unique_ptr<RTCRtpSenderImpl> CreateSender(
      MediaStreamComponent* component) {
    std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
        track_ref;
    if (component) {
      track_ref = track_map_->GetOrCreateLocalTrackAdapter(component);
      DCHECK(track_ref->is_initialized());
    }
    RtpSenderState sender_state(
        main_thread_, dependency_factory_->GetWebRtcSignalingTaskRunner(),
        mock_webrtc_sender_, std::move(track_ref), std::vector<std::string>());
    sender_state.Initialize();
    return std::make_unique<RTCRtpSenderImpl>(peer_connection_, track_map_,
                                              std::move(sender_state));
  }

  // Calls replaceTrack(), which is asynchronous, returning a callback that when
  // invoked waits for (run-loops) the operation to complete and returns whether
  // replaceTrack() was successful.
  base::OnceCallback<bool()> ReplaceTrack(MediaStreamComponent* component) {
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    std::unique_ptr<bool> result_holder(new bool());
    // On complete, |*result_holder| is set with the result of replaceTrack()
    // and the |run_loop| quit.
    sender_->ReplaceTrack(
        component,
        CrossThreadBindOnce(&RTCRtpSenderImplTest::CallbackOnComplete,
                            Unretained(this), Unretained(result_holder.get()),
                            blink::Unretained(run_loop.get())));
    // When the resulting callback is invoked, waits for |run_loop| to complete
    // and returns |*result_holder|.
    return base::BindOnce(&RTCRtpSenderImplTest::RunLoopAndReturnResult,
                          base::Unretained(this), std::move(result_holder),
                          std::move(run_loop));
  }

  scoped_refptr<blink::TestWebRTCStatsReportObtainer> CallGetStats() {
    scoped_refptr<blink::TestWebRTCStatsReportObtainer> obtainer =
        base::MakeRefCounted<TestWebRTCStatsReportObtainer>();
    sender_->GetStats(obtainer->GetStatsCallbackWrapper());
    return obtainer;
  }

 protected:
  void CallbackOnComplete(bool* result_out,
                          base::RunLoop* run_loop,
                          bool result) {
    *result_out = result;
    run_loop->Quit();
  }

  bool RunLoopAndReturnResult(std::unique_ptr<bool> result_holder,
                              std::unique_ptr<base::RunLoop> run_loop) {
    run_loop->Run();
    return *result_holder;
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  Persistent<MockPeerConnectionDependencyFactory> dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_map_;
  webrtc::scoped_refptr<blink::MockPeerConnectionImpl> peer_connection_;
  webrtc::scoped_refptr<MockRtpSender> mock_webrtc_sender_;
  std::unique_ptr<RTCRtpSenderImpl> sender_;
};

TEST_F(RTCRtpSenderImplTest, CreateSender) {
  auto* component = CreateTrack("track_id");
  sender_ = CreateSender(component);
  EXPECT_TRUE(sender_->Track());
  EXPECT_EQ(component->UniqueId(), sender_->Track()->UniqueId());

  // Encoded audio transformer should be created.
  EXPECT_TRUE(sender_->GetEncodedAudioStreamTransformer());
  // There should be no video transformer in audio senders.
  EXPECT_FALSE(sender_->GetEncodedVideoStreamTransformer());
}

TEST_F(RTCRtpSenderImplTest, CreateSenderWithNullTrack) {
  MediaStreamComponent* null_component = nullptr;
  sender_ = CreateSender(null_component);
  EXPECT_FALSE(sender_->Track());
}

TEST_F(RTCRtpSenderImplTest, CreateSframeEncryptorForwardsConfiguration) {
  sender_ = CreateSender(/*component=*/nullptr);
  RTCRtpSenderPlatform& platform_sender = *sender_;
  constexpr webrtc::SframeEncryptorInit kConfigurations[] = {
      {.mode = webrtc::SframeMode::kPerFrame,
       .cipher_suite = webrtc::SframeCipherSuite::kAes128GcmSha256_128},
      {.mode = webrtc::SframeMode::kPerPacket,
       .cipher_suite = webrtc::SframeCipherSuite::kAes256CtrHmacSha512_80},
  };
  for (const webrtc::SframeEncryptorInit& init : kConfigurations) {
    EXPECT_CALL(
        *mock_webrtc_sender_,
        CreateSframeEncryptorOrError(testing::AllOf(
            testing::Field(&webrtc::SframeEncryptorInit::mode, init.mode),
            testing::Field(&webrtc::SframeEncryptorInit::cipher_suite,
                           init.cipher_suite))))
        .WillOnce(Return(webrtc::RTCError::InternalError()));

    // A known native error confirms that the request reaches WebRTC.
    webrtc::RTCErrorOr<scoped_refptr<webrtc::SframeEncryptorInterface>> result =
        platform_sender.CreateSframeEncryptor(init);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.error().type(), webrtc::RTCErrorType::INTERNAL_ERROR);
  }
}

TEST_F(RTCRtpSenderImplTest, CreateSframeEncryptorPreservesNativeError) {
  sender_ = CreateSender(/*component=*/nullptr);
  RTCRtpSenderPlatform& platform_sender = *sender_;
  const webrtc::SframeEncryptorInit init{
      .mode = webrtc::SframeMode::kPerFrame,
      .cipher_suite = webrtc::SframeCipherSuite::kAes128GcmSha256_128};
  EXPECT_CALL(*mock_webrtc_sender_, CreateSframeEncryptorOrError(_))
      .WillOnce(Return(webrtc::RTCError::InvalidState("Sender is stopped")));

  webrtc::RTCErrorOr<scoped_refptr<webrtc::SframeEncryptorInterface>> result =
      platform_sender.CreateSframeEncryptor(init);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.error().type(), webrtc::RTCErrorType::INVALID_STATE);
  EXPECT_STREQ(result.error().message(), "Sender is stopped");
}

TEST_F(RTCRtpSenderImplTest, CreateSframeEncryptorReturnsNativeEncryptor) {
  sender_ = CreateSender(/*component=*/nullptr);
  RTCRtpSenderPlatform& platform_sender = *sender_;
  const webrtc::SframeEncryptorInit init{
      .mode = webrtc::SframeMode::kPerFrame,
      .cipher_suite = webrtc::SframeCipherSuite::kAes128GcmSha256_128};
  webrtc::scoped_refptr<webrtc::SframeEncryptorInterface> native_encryptor =
      webrtc::make_ref_counted<FakeSframeEncryptor>();
  EXPECT_CALL(*mock_webrtc_sender_, CreateSframeEncryptorOrError(_))
      .WillOnce(Return(native_encryptor));

  webrtc::RTCErrorOr<scoped_refptr<webrtc::SframeEncryptorInterface>> result =
      platform_sender.CreateSframeEncryptor(init);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.value().get(), native_encryptor.get());
}

TEST_F(RTCRtpSenderImplTest, ReplaceTrackSetsTrack) {
  auto* component1 = CreateTrack("track1");
  sender_ = CreateSender(component1);

  auto* component2 = CreateTrack("track2");
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(true));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(component2);
  EXPECT_TRUE(std::move(replaceTrackRunLoopAndGetResult).Run());
  ASSERT_TRUE(sender_->Track());
  EXPECT_EQ(component2->UniqueId(), sender_->Track()->UniqueId());
}

TEST_F(RTCRtpSenderImplTest, ReplaceTrackWithNullTrack) {
  auto* component = CreateTrack("track_id");
  sender_ = CreateSender(component);

  MediaStreamComponent* null_component = nullptr;
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(true));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(null_component);
  EXPECT_TRUE(std::move(replaceTrackRunLoopAndGetResult).Run());
  EXPECT_FALSE(sender_->Track());
}

TEST_F(RTCRtpSenderImplTest, ReplaceTrackCanFail) {
  auto* component = CreateTrack("track_id");
  sender_ = CreateSender(component);
  ASSERT_TRUE(sender_->Track());
  EXPECT_EQ(component->UniqueId(), sender_->Track()->UniqueId());

  MediaStreamComponent* null_component = nullptr;
  ;
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(false));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(null_component);
  EXPECT_FALSE(std::move(replaceTrackRunLoopAndGetResult).Run());
  // The track should not have been set.
  ASSERT_TRUE(sender_->Track());
  EXPECT_EQ(component->UniqueId(), sender_->Track()->UniqueId());
}

TEST_F(RTCRtpSenderImplTest, ReplaceTrackIsNotSetSynchronously) {
  auto* component1 = CreateTrack("track1");
  sender_ = CreateSender(component1);

  auto* component2 = CreateTrack("track2");
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(true));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(component2);
  // The track should not be set until the run loop has executed.
  ASSERT_TRUE(sender_->Track());
  EXPECT_NE(component2->UniqueId(), sender_->Track()->UniqueId());
  // Wait for operation to run to ensure EXPECT_CALL is satisfied.
  std::move(replaceTrackRunLoopAndGetResult).Run();
}

TEST_F(RTCRtpSenderImplTest, GetStats) {
  auto* component = CreateTrack("track_id");
  sender_ = CreateSender(component);

  // Make the mock return a blink version of the |webtc_report|. The mock does
  // not perform any stats filtering, we just set it to a dummy value.
  webrtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(0));
  webrtc_report->AddStats(std::make_unique<webrtc::RTCOutboundRtpStreamStats>(
      "stats-id", webrtc::Timestamp::Micros(1234)));
  peer_connection_->SetGetStatsReport(webrtc_report.get());

  auto obtainer = CallGetStats();
  // Make sure the operation is async.
  EXPECT_FALSE(obtainer->report());
  // Wait for the report, this performs the necessary run-loop.
  auto* report = obtainer->WaitForReport();
  EXPECT_TRUE(report);
}

TEST_F(RTCRtpSenderImplTest, CopiedSenderSharesInternalStates) {
  auto* component = CreateTrack("track_id");
  sender_ = CreateSender(component);
  auto copy = std::make_unique<RTCRtpSenderImpl>(*sender_);
  // Copy shares original's ID.
  EXPECT_EQ(sender_->Id(), copy->Id());

  MediaStreamComponent* null_component = nullptr;
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(true));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(null_component);
  EXPECT_TRUE(std::move(replaceTrackRunLoopAndGetResult).Run());

  // Both original and copy shows a modified state.
  EXPECT_FALSE(sender_->Track());
  EXPECT_FALSE(copy->Track());
}

TEST_F(RTCRtpSenderImplTest,
       GenerateKeyFrameSelectsActiveLayersAndNotifiesBeforeEncoder) {
  auto* component = CreateTrack("track_id");
  sender_ = CreateSender(component);

  webrtc::RtpParameters params;
  webrtc::RtpEncodingParameters layer0;
  layer0.rid = "0";
  layer0.active = true;
  webrtc::RtpEncodingParameters layer1;
  layer1.rid = "1";
  layer1.active = false;
  webrtc::RtpEncodingParameters layer2;
  layer2.rid = "2";
  layer2.active = true;
  params.encodings = {layer0, layer1, layer2};

  EXPECT_CALL(*mock_webrtc_sender_, GetParameters())
      .WillRepeatedly(Return(params));

  bool layers_notified = false;
  Vector<String> selected_rids;
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_webrtc_sender_,
              GenerateKeyFrame(testing::ElementsAre("0", "2")))
      .WillOnce([&](const std::vector<std::string>&) {
        EXPECT_TRUE(layers_notified);
        run_loop.Quit();
        return webrtc::RTCError::OK();
      });

  sender_->GenerateKeyFrame(
      /*rid=*/String(),
      CrossThreadBindOnce(
          [](bool* notified, Vector<String>* out_rids, Vector<String> rids) {
            *notified = true;
            *out_rids = std::move(rids);
          },
          CrossThreadUnretained(&layers_notified),
          CrossThreadUnretained(&selected_rids)),
      CrossThreadBindOnce([](webrtc::RTCError) { FAIL(); }));
  run_loop.Run();
  EXPECT_THAT(selected_rids, testing::ElementsAre("0", "2"));

  // Requesting an inactive layer reports empty rids and does not call WebRTC.
  EXPECT_CALL(*mock_webrtc_sender_, GenerateKeyFrame(_)).Times(0);
  base::RunLoop inactive_loop;
  Vector<String> inactive_rids;
  sender_->GenerateKeyFrame(
      "1",
      CrossThreadBindOnce(
          [](Vector<String>* out_rids, base::RepeatingClosure quit,
             Vector<String> rids) {
            *out_rids = std::move(rids);
            quit.Run();
          },
          CrossThreadUnretained(&inactive_rids), inactive_loop.QuitClosure()),
      CrossThreadBindOnce([](webrtc::RTCError) { FAIL(); }));
  inactive_loop.Run();
  EXPECT_TRUE(inactive_rids.empty());
}

TEST_F(RTCRtpSenderImplTest, GenerateKeyFrameReportsEncoderError) {
  auto* component = CreateTrack("track_id");
  sender_ = CreateSender(component);

  webrtc::RtpParameters params;
  webrtc::RtpEncodingParameters layer0;
  layer0.rid = "0";
  layer0.active = true;
  params.encodings = {layer0};

  EXPECT_CALL(*mock_webrtc_sender_, GetParameters()).WillOnce(Return(params));
  EXPECT_CALL(*mock_webrtc_sender_, GenerateKeyFrame(testing::ElementsAre("0")))
      .WillOnce(
          Return(webrtc::RTCError(webrtc::RTCErrorType::INVALID_PARAMETER)));

  base::RunLoop run_loop;
  webrtc::RTCErrorType received_error = webrtc::RTCErrorType::NONE;
  sender_->GenerateKeyFrame(
      "0", CrossThreadBindOnce([](Vector<String>) {}),
      CrossThreadBindOnce(
          [](webrtc::RTCErrorType* out_type, base::RepeatingClosure quit,
             webrtc::RTCError error) {
            *out_type = error.type();
            quit.Run();
          },
          CrossThreadUnretained(&received_error), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(received_error, webrtc::RTCErrorType::INVALID_PARAMETER);
}

}  // namespace blink
