// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver_impl.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/test_webrtc_stats_report_obtainer.h"
#include "third_party/blink/renderer/modules/peerconnection/testing/mock_rtp_receiver.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"

namespace blink {

class RTCRtpReceiverImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_ =
        MakeGarbageCollected<MockPeerConnectionDependencyFactory>();
    main_thread_ = blink::scheduler::GetSingleThreadTaskRunnerForTesting();
    track_map_ = base::MakeRefCounted<blink::WebRtcMediaStreamTrackAdapterMap>(
        dependency_factory_.Get(), main_thread_);
    peer_connection_ = new rtc::RefCountedObject<blink::MockPeerConnectionImpl>(
        dependency_factory_.Get(), nullptr);
  }

  void TearDown() override {
    receiver_.reset();
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

  std::unique_ptr<RTCRtpReceiverImpl> CreateReceiver(
      scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track,
      bool require_encoded_insertable_streams = false) {
    std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
        track_ref;
    base::RunLoop run_loop;
    dependency_factory_->GetWebRtcSignalingTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&RTCRtpReceiverImplTest::CreateReceiverOnSignalingThread,
                       base::Unretained(this), std::move(webrtc_track),
                       base::Unretained(&track_ref),
                       base::Unretained(&run_loop)));
    run_loop.Run();
    DCHECK(mock_webrtc_receiver_);
    DCHECK(track_ref);
    blink::RtpReceiverState state(
        main_thread_, dependency_factory_->GetWebRtcSignalingTaskRunner(),
        mock_webrtc_receiver_.get(), std::move(track_ref), {});
    state.Initialize();
    return std::make_unique<RTCRtpReceiverImpl>(
        peer_connection_, std::move(state), require_encoded_insertable_streams,
        /*decode_metronome=*/nullptr);
  }

  scoped_refptr<blink::TestWebRTCStatsReportObtainer> GetStats() {
    scoped_refptr<blink::TestWebRTCStatsReportObtainer> obtainer =
        base::MakeRefCounted<TestWebRTCStatsReportObtainer>();
    receiver_->GetStats(obtainer->GetStatsCallbackWrapper());
    return obtainer;
  }

 protected:
  void CreateReceiverOnSignalingThread(
      scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track,
      std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>*
          track_ref,
      base::RunLoop* run_loop) {
    mock_webrtc_receiver_ = new rtc::RefCountedObject<MockRtpReceiver>();
    *track_ref = track_map_->GetOrCreateRemoteTrackAdapter(webrtc_track);
    run_loop->Quit();
  }

  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  Persistent<blink::MockPeerConnectionDependencyFactory> dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_map_;
  rtc::scoped_refptr<blink::MockPeerConnectionImpl> peer_connection_;
  rtc::scoped_refptr<MockRtpReceiver> mock_webrtc_receiver_;
  std::unique_ptr<RTCRtpReceiverImpl> receiver_;
};

TEST_F(RTCRtpReceiverImplTest, CreateReceiver) {
  scoped_refptr<blink::MockWebRtcAudioTrack> webrtc_track =
      blink::MockWebRtcAudioTrack::Create("webrtc_track");
  receiver_ = CreateReceiver(webrtc_track);
  EXPECT_FALSE(!receiver_->Track());
  EXPECT_EQ(receiver_->Track()->Id().Utf8(), webrtc_track->id());
  EXPECT_EQ(receiver_->state().track_ref()->webrtc_track().get(),
            webrtc_track.get());

  EXPECT_TRUE(receiver_->GetEncodedAudioStreamTransformer());
  EXPECT_FALSE(receiver_->GetEncodedVideoStreamTransformer());
}

TEST_F(RTCRtpReceiverImplTest, ShallowCopy) {
  scoped_refptr<blink::MockWebRtcAudioTrack> webrtc_track =
      blink::MockWebRtcAudioTrack::Create("webrtc_track");
  receiver_ = CreateReceiver(webrtc_track);
  auto copy = std::make_unique<RTCRtpReceiverImpl>(*receiver_);
  EXPECT_EQ(receiver_->state().track_ref()->webrtc_track().get(),
            webrtc_track.get());
  const auto& webrtc_receiver = receiver_->state().webrtc_receiver();
  auto web_track_unique_id = receiver_->Track()->UniqueId();
  // Copy is identical to original.
  EXPECT_EQ(copy->state().webrtc_receiver(), webrtc_receiver);
  EXPECT_EQ(copy->state().track_ref()->webrtc_track().get(),
            webrtc_track.get());
  EXPECT_EQ(copy->Track()->UniqueId(), web_track_unique_id);
  // Copy keeps the internal state alive.
  receiver_.reset();
  EXPECT_EQ(copy->state().webrtc_receiver(), webrtc_receiver);
  EXPECT_EQ(copy->state().track_ref()->webrtc_track().get(),
            webrtc_track.get());
  EXPECT_EQ(copy->Track()->UniqueId(), web_track_unique_id);
}

TEST_F(RTCRtpReceiverImplTest, GetStats) {
  scoped_refptr<blink::MockWebRtcAudioTrack> webrtc_track =
      blink::MockWebRtcAudioTrack::Create("webrtc_track");
  receiver_ = CreateReceiver(webrtc_track);

  // Make the mock return a blink version of the |webtc_report|. The mock does
  // not perform any stats filtering, we just set it to a dummy value.
  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(webrtc::Timestamp::Micros(0));
  webrtc_report->AddStats(std::make_unique<webrtc::RTCInboundRtpStreamStats>(
      "stats-id", webrtc::Timestamp::Micros(1234)));
  peer_connection_->SetGetStatsReport(webrtc_report.get());

  auto obtainer = GetStats();
  // Make sure the operation is async.
  EXPECT_FALSE(obtainer->report());
  // Wait for the report, this performs the necessary run-loop.
  auto* report = obtainer->WaitForReport();
  EXPECT_TRUE(report);
}

TEST_F(RTCRtpReceiverImplTest, CreateReceiverWithInsertableStreams) {
  scoped_refptr<blink::MockWebRtcAudioTrack> webrtc_track =
      blink::MockWebRtcAudioTrack::Create("webrtc_track");
  receiver_ = CreateReceiver(webrtc_track, true);
  EXPECT_TRUE(receiver_->GetEncodedAudioStreamTransformer());
  // There should be no video transformer in audio receivers.
  EXPECT_FALSE(receiver_->GetEncodedVideoStreamTransformer());
}

}  // namespace blink
