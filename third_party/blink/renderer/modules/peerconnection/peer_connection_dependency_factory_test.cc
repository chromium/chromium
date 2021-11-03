// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/peerconnection/execution_context_metronome_provider.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/webrtc_overrides/metronome_provider.h"
#include "third_party/webrtc_overrides/metronome_task_queue_factory.h"

namespace blink {

namespace {

class FakeMetronomeProviderListener : public MetronomeProviderListener {
 public:
  size_t start_count() const { return start_count_; }
  size_t stop_count() const { return stop_count_; }

  // MetronomeProviderListener:
  void OnStartUsingMetronome(
      scoped_refptr<MetronomeSource> metronome) override {
    DCHECK(!metronome_);
    metronome_ = metronome;
    ++start_count_;
  }
  void OnStopUsingMetronome() override {
    DCHECK(metronome_);
    metronome_ = nullptr;
    ++stop_count_;
  }

 private:
  scoped_refptr<MetronomeSource> metronome_;
  size_t start_count_ = 0;
  size_t stop_count_ = 0;
};

}  // namespace

class PeerConnectionDependencyFactoryTest : public ::testing::Test {
 public:
  void EnsureDependencyFactory(ExecutionContext& context) {
    dependency_factory_ = &PeerConnectionDependencyFactory::From(context);
    ASSERT_TRUE(dependency_factory_);
  }

  std::unique_ptr<RTCPeerConnectionHandler> CreateRTCPeerConnectionHandler() {
    std::unique_ptr<RTCPeerConnectionHandler> handler =
        dependency_factory_->CreateRTCPeerConnectionHandler(
            &mock_client_,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*force_encoded_audio_insertable_streams=*/false,
            /*force_encoded_video_insertable_streams=*/false);
    MediaConstraints constraints;
    DummyExceptionStateForTesting exception_state;
    handler->InitializeForTest(
        webrtc::PeerConnectionInterface::RTCConfiguration(), constraints,
        /*peer_connection_tracker=*/nullptr, exception_state);
    return handler;
  }

 protected:
  Persistent<PeerConnectionDependencyFactory> dependency_factory_;
  MockRTCPeerConnectionHandlerClient mock_client_;
};

TEST_F(PeerConnectionDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  V8TestingScope scope;
  EnsureDependencyFactory(*scope.GetExecutionContext());

  std::unique_ptr<RTCPeerConnectionHandler> pc_handler =
      CreateRTCPeerConnectionHandler();
  EXPECT_TRUE(pc_handler);
}

TEST_F(PeerConnectionDependencyFactoryTest, CountOpenPeerConnections) {
  V8TestingScope scope;
  EnsureDependencyFactory(*scope.GetExecutionContext());

  EXPECT_EQ(dependency_factory_->open_peer_connections(), 0u);
  std::unique_ptr<RTCPeerConnectionHandler> pc1 =
      CreateRTCPeerConnectionHandler();
  EXPECT_EQ(dependency_factory_->open_peer_connections(), 1u);
  std::unique_ptr<RTCPeerConnectionHandler> pc2 =
      CreateRTCPeerConnectionHandler();
  EXPECT_EQ(dependency_factory_->open_peer_connections(), 2u);
  pc1->Close();
  EXPECT_EQ(dependency_factory_->open_peer_connections(), 1u);
  pc2->Close();
  EXPECT_EQ(dependency_factory_->open_peer_connections(), 0u);
  std::unique_ptr<RTCPeerConnectionHandler> pc3 =
      CreateRTCPeerConnectionHandler();
  EXPECT_EQ(dependency_factory_->open_peer_connections(), 1u);
  pc3->Close();
  EXPECT_EQ(dependency_factory_->open_peer_connections(), 0u);
}

TEST_F(PeerConnectionDependencyFactoryTest,
       ShouldNotUseMetronomeWhenFeatureIsDisabled) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(kWebRtcMetronomeTaskQueue));

  V8TestingScope scope;
  EnsureDependencyFactory(*scope.GetExecutionContext());

  FakeMetronomeProviderListener fake_metronome_listener;
  ExecutionContextMetronomeProvider::From(*scope.GetExecutionContext())
      .metronome_provider()
      ->AddListener(&fake_metronome_listener);

  // Start using WebRTC. Nothing should happen to the metronome use counters.
  std::unique_ptr<RTCPeerConnectionHandler> pc1 =
      CreateRTCPeerConnectionHandler();
  EXPECT_EQ(fake_metronome_listener.start_count(), 0u);
  EXPECT_EQ(fake_metronome_listener.stop_count(), 0u);

  ExecutionContextMetronomeProvider::From(*scope.GetExecutionContext())
      .metronome_provider()
      ->RemoveListener(&fake_metronome_listener);
}

TEST_F(PeerConnectionDependencyFactoryTest,
       ShouldUseMetronomeWhenThereAreOpenPeerConnectionsAndFeatureIsEnabled) {
  base::test::ScopedFeatureList feature_list(kWebRtcMetronomeTaskQueue);

  V8TestingScope scope;
  EnsureDependencyFactory(*scope.GetExecutionContext());

  FakeMetronomeProviderListener fake_metronome_listener;
  ExecutionContextMetronomeProvider::From(*scope.GetExecutionContext())
      .metronome_provider()
      ->AddListener(&fake_metronome_listener);

  EXPECT_EQ(fake_metronome_listener.start_count(), 0u);
  EXPECT_EQ(fake_metronome_listener.stop_count(), 0u);

  // Create a peer connection, the start count should increment.
  std::unique_ptr<RTCPeerConnectionHandler> pc1 =
      CreateRTCPeerConnectionHandler();
  EXPECT_EQ(fake_metronome_listener.start_count(), 1u);
  EXPECT_EQ(fake_metronome_listener.stop_count(), 0u);

  // Create another peer connection. The start count remains the same.
  std::unique_ptr<RTCPeerConnectionHandler> pc2 =
      CreateRTCPeerConnectionHandler();
  EXPECT_EQ(fake_metronome_listener.start_count(), 1u);
  EXPECT_EQ(fake_metronome_listener.stop_count(), 0u);

  // When the metronome is already in use, adding another listener should
  // immediately inform it that there is a metronome that may be used.
  FakeMetronomeProviderListener fake_metronome_listener2;
  ExecutionContextMetronomeProvider::From(*scope.GetExecutionContext())
      .metronome_provider()
      ->AddListener(&fake_metronome_listener2);
  EXPECT_EQ(fake_metronome_listener2.start_count(), 1u);
  EXPECT_EQ(fake_metronome_listener2.stop_count(), 0u);
  ExecutionContextMetronomeProvider::From(*scope.GetExecutionContext())
      .metronome_provider()
      ->RemoveListener(&fake_metronome_listener2);

  // Stop a peer connection. The stop count does not increase because there is
  // one more non-stopped peer connection.
  pc1->Close();
  EXPECT_EQ(fake_metronome_listener.start_count(), 1u);
  EXPECT_EQ(fake_metronome_listener.stop_count(), 0u);

  // The stop count increases when there are no more peer connections.
  pc2->Close();
  EXPECT_EQ(fake_metronome_listener.start_count(), 1u);
  EXPECT_EQ(fake_metronome_listener.stop_count(), 1u);

  // The previously removed listener was not informed.
  EXPECT_EQ(fake_metronome_listener2.start_count(), 1u);
  EXPECT_EQ(fake_metronome_listener2.stop_count(), 0u);

  // Creating another peer connection should start it again.
  std::unique_ptr<RTCPeerConnectionHandler> pc3 =
      CreateRTCPeerConnectionHandler();
  EXPECT_EQ(fake_metronome_listener.start_count(), 2u);
  EXPECT_EQ(fake_metronome_listener.stop_count(), 1u);
  pc3->Close();
  EXPECT_EQ(fake_metronome_listener.start_count(), 2u);
  EXPECT_EQ(fake_metronome_listener.stop_count(), 2u);
  ExecutionContextMetronomeProvider::From(*scope.GetExecutionContext())
      .metronome_provider()
      ->RemoveListener(&fake_metronome_listener);
}

}  // namespace blink
