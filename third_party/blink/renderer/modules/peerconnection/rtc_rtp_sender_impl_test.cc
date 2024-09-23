// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/test_webrtc_stats_report_obtainer.h"
#include "third_party/blink/renderer/modules/peerconnection/testing/mock_rtp_sender.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"

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
    peer_connection_ = new rtc::RefCountedObject<blink::MockPeerConnectionImpl>(
        dependency_factory_.Get(), nullptr);
    mock_webrtc_sender_ = new rtc::RefCountedObject<MockRtpSender>();
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
        String::FromUTF8(id), MediaStreamSource::kTypeAudio,
        String::FromUTF8("local_audio_track"), false, std::move(audio_source));

    auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
        source->Id(), source,
        std::make_unique<MediaStreamAudioTrack>(/*is_local=*/true));
    audio_source_ptr->ConnectToInitializedTrack(component);
    return component;
  }

  std::unique_ptr<RTCRtpSenderImpl> CreateSender(
      MediaStreamComponent* component,
      bool require_encoded_insertable_streams = false) {
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
    return std::make_unique<RTCRtpSenderImpl>(
        peer_connection_, track_map_, std::move(sender_state),
        require_encoded_insertable_streams);
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
        component, WTF::BindOnce(&RTCRtpSenderImplTest::CallbackOnComplete,
                                 WTF::Unretained(this),
                                 WTF::Unretained(result_holder.get()),
                                 WTF::Unretained(run_loop.get())));
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
  rtc::scoped_refptr<blink::MockPeerConnectionImpl> peer_connection_;
  rtc::scoped_refptr<MockRtpSender> mock_webrtc_sender_;
  std::unique_ptr<RTCRtpSenderImpl> sender_;
};

TEST_F(RTCRtpSenderImplTest, CreateSender) {
  auto* component = CreateTrack("track_id");
  sender_ = CreateSender(component);
  EXPECT_TRUE(sender_->Track());
  EXPECT_EQ(component->UniqueId(), sender_->Track()->UniqueId());
}

TEST_F(RTCRtpSenderImplTest, CreateSenderWithNullTrack) {
  MediaStreamComponent* null_component = nullptr;
  sender_ = CreateSender(null_component);
  EXPECT_FALSE(sender_->Track());
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
  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
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

TEST_F(RTCRtpSenderImplTest, CreateSenderWithInsertableStreams) {
  auto* component = CreateTrack("track_id");
  sender_ = CreateSender(component,
                         /*require_encoded_insertable_streams=*/true);
  EXPECT_TRUE(sender_->GetEncodedAudioStreamTransformer());
  // There should be no video transformer in audio senders.
  EXPECT_FALSE(sender_->GetEncodedVideoStreamTransformer());
}

}  // namespace blink
