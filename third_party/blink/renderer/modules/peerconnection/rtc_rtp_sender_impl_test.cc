// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"
#include "third_party/blink/public/platform/web_rtc_stats.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/test_webrtc_stats_report_obtainer.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_stats.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_void_request.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/webrtc/api/stats/rtc_stats_report.h"
#include "third_party/webrtc/api/stats/rtcstats_objects.h"
#include "third_party/webrtc/api/test/mock_rtpsender.h"

using ::testing::_;
using ::testing::Return;

namespace blink {

class RTCRtpSenderImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_.reset(new blink::MockPeerConnectionDependencyFactory());
    main_thread_ = blink::scheduler::GetSingleThreadTaskRunnerForTesting();
    track_map_ = base::MakeRefCounted<blink::WebRtcMediaStreamTrackAdapterMap>(
        dependency_factory_.get(), main_thread_);
    peer_connection_ = new rtc::RefCountedObject<blink::MockPeerConnectionImpl>(
        dependency_factory_.get(), nullptr);
    mock_webrtc_sender_ = new rtc::RefCountedObject<webrtc::MockRtpSender>();
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

  blink::WebMediaStreamTrack CreateWebTrack(const std::string& id) {
    blink::WebMediaStreamSource web_source;
    web_source.Initialize(
        blink::WebString::FromUTF8(id), blink::WebMediaStreamSource::kTypeAudio,
        blink::WebString::FromUTF8("local_audio_track"), false);
    blink::MediaStreamAudioSource* audio_source =
        new blink::MediaStreamAudioSource(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(), true);
    // Takes ownership of |audio_source|.
    web_source.SetPlatformSource(base::WrapUnique(audio_source));
    blink::WebMediaStreamTrack web_track;
    web_track.Initialize(web_source.Id(), web_source);
    audio_source->ConnectToTrack(web_track);
    return web_track;
  }

  std::unique_ptr<RTCRtpSenderImpl> CreateSender(
      blink::WebMediaStreamTrack web_track) {
    std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
        track_ref;
    if (!web_track.IsNull()) {
      track_ref = track_map_->GetOrCreateLocalTrackAdapter(web_track);
      DCHECK(track_ref->is_initialized());
    }
    RtpSenderState sender_state(
        main_thread_, dependency_factory_->GetWebRtcSignalingTaskRunner(),
        mock_webrtc_sender_.get(), std::move(track_ref),
        std::vector<std::string>());
    sender_state.Initialize();
    return std::make_unique<RTCRtpSenderImpl>(
        peer_connection_.get(), track_map_, std::move(sender_state));
  }

  // Calls replaceTrack(), which is asynchronous, returning a callback that when
  // invoked waits for (run-loops) the operation to complete and returns whether
  // replaceTrack() was successful.
  base::OnceCallback<bool()> ReplaceTrack(
      blink::WebMediaStreamTrack web_track) {
    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    std::unique_ptr<bool> result_holder(new bool());
    // On complete, |*result_holder| is set with the result of replaceTrack()
    // and the |run_loop| quit.
    sender_->ReplaceTrack(
        web_track, base::BindOnce(&RTCRtpSenderImplTest::CallbackOnComplete,
                                  base::Unretained(this), result_holder.get(),
                                  run_loop.get()));
    // When the resulting callback is invoked, waits for |run_loop| to complete
    // and returns |*result_holder|.
    return base::BindOnce(&RTCRtpSenderImplTest::RunLoopAndReturnResult,
                          base::Unretained(this), std::move(result_holder),
                          std::move(run_loop));
  }

  scoped_refptr<blink::TestWebRTCStatsReportObtainer> CallGetStats() {
    scoped_refptr<blink::TestWebRTCStatsReportObtainer> obtainer =
        base::MakeRefCounted<TestWebRTCStatsReportObtainer>();
    sender_->GetStats(obtainer->GetStatsCallbackWrapper(), {});
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

  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  std::unique_ptr<blink::MockPeerConnectionDependencyFactory>
      dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_map_;
  rtc::scoped_refptr<blink::MockPeerConnectionImpl> peer_connection_;
  rtc::scoped_refptr<webrtc::MockRtpSender> mock_webrtc_sender_;
  std::unique_ptr<RTCRtpSenderImpl> sender_;
};

TEST_F(RTCRtpSenderImplTest, CreateSender) {
  auto web_track = CreateWebTrack("track_id");
  sender_ = CreateSender(web_track);
  EXPECT_FALSE(sender_->Track().IsNull());
  EXPECT_EQ(web_track.UniqueId(), sender_->Track().UniqueId());
}

TEST_F(RTCRtpSenderImplTest, CreateSenderWithNullTrack) {
  blink::WebMediaStreamTrack null_track;
  sender_ = CreateSender(null_track);
  EXPECT_TRUE(sender_->Track().IsNull());
}

TEST_F(RTCRtpSenderImplTest, ReplaceTrackSetsTrack) {
  auto web_track1 = CreateWebTrack("track1");
  sender_ = CreateSender(web_track1);

  auto web_track2 = CreateWebTrack("track2");
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(true));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(web_track2);
  EXPECT_TRUE(std::move(replaceTrackRunLoopAndGetResult).Run());
  ASSERT_FALSE(sender_->Track().IsNull());
  EXPECT_EQ(web_track2.UniqueId(), sender_->Track().UniqueId());
}

TEST_F(RTCRtpSenderImplTest, ReplaceTrackWithNullTrack) {
  auto web_track = CreateWebTrack("track_id");
  sender_ = CreateSender(web_track);

  blink::WebMediaStreamTrack null_track;
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(true));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(null_track);
  EXPECT_TRUE(std::move(replaceTrackRunLoopAndGetResult).Run());
  EXPECT_TRUE(sender_->Track().IsNull());
}

TEST_F(RTCRtpSenderImplTest, ReplaceTrackCanFail) {
  auto web_track = CreateWebTrack("track_id");
  sender_ = CreateSender(web_track);
  ASSERT_FALSE(sender_->Track().IsNull());
  EXPECT_EQ(web_track.UniqueId(), sender_->Track().UniqueId());

  blink::WebMediaStreamTrack null_track;
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(false));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(null_track);
  EXPECT_FALSE(std::move(replaceTrackRunLoopAndGetResult).Run());
  // The track should not have been set.
  ASSERT_FALSE(sender_->Track().IsNull());
  EXPECT_EQ(web_track.UniqueId(), sender_->Track().UniqueId());
}

TEST_F(RTCRtpSenderImplTest, ReplaceTrackIsNotSetSynchronously) {
  auto web_track1 = CreateWebTrack("track1");
  sender_ = CreateSender(web_track1);

  auto web_track2 = CreateWebTrack("track2");
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(true));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(web_track2);
  // The track should not be set until the run loop has executed.
  ASSERT_FALSE(sender_->Track().IsNull());
  EXPECT_NE(web_track2.UniqueId(), sender_->Track().UniqueId());
  // Wait for operation to run to ensure EXPECT_CALL is satisfied.
  std::move(replaceTrackRunLoopAndGetResult).Run();
}

TEST_F(RTCRtpSenderImplTest, GetStats) {
  auto web_track = CreateWebTrack("track_id");
  sender_ = CreateSender(web_track);

  // Make the mock return a blink version of the |webtc_report|. The mock does
  // not perform any stats filtering, we just set it to a dummy value.
  rtc::scoped_refptr<webrtc::RTCStatsReport> webrtc_report =
      webrtc::RTCStatsReport::Create(0u);
  webrtc_report->AddStats(
      std::make_unique<webrtc::RTCOutboundRTPStreamStats>("stats-id", 1234u));
  peer_connection_->SetGetStatsReport(webrtc_report);

  auto obtainer = CallGetStats();
  // Make sure the operation is async.
  EXPECT_FALSE(obtainer->report());
  // Wait for the report, this performs the necessary run-loop.
  auto* report = obtainer->WaitForReport();
  EXPECT_TRUE(report);

  // Verify dummy value.
  EXPECT_EQ(report->Size(), 1u);
  auto stats = report->GetStats(blink::WebString::FromUTF8("stats-id"));
  EXPECT_TRUE(stats);
  EXPECT_EQ(stats->Timestamp(), 1.234);
}

TEST_F(RTCRtpSenderImplTest, CopiedSenderSharesInternalStates) {
  auto web_track = CreateWebTrack("track_id");
  sender_ = CreateSender(web_track);
  auto copy = std::make_unique<RTCRtpSenderImpl>(*sender_);
  // Copy shares original's ID.
  EXPECT_EQ(sender_->Id(), copy->Id());

  blink::WebMediaStreamTrack null_track;
  EXPECT_CALL(*mock_webrtc_sender_, SetTrack(_)).WillOnce(Return(true));
  auto replaceTrackRunLoopAndGetResult = ReplaceTrack(null_track);
  EXPECT_TRUE(std::move(replaceTrackRunLoopAndGetResult).Run());

  // Both original and copy shows a modified state.
  EXPECT_TRUE(sender_->Track().IsNull());
  EXPECT_TRUE(copy->Track().IsNull());
}

}  // namespace blink
