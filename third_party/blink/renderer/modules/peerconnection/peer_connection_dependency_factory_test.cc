// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"

#include <memory>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

using network::mojom::IPAddressSpace;

namespace {

webrtc::SocketAddress AddressForSpace(IPAddressSpace address_space) {
  switch (address_space) {
    case IPAddressSpace::kLoopback:
      return webrtc::SocketAddress("127.0.0.1", 1234);
    case IPAddressSpace::kLocal:
      return webrtc::SocketAddress("192.168.1.1", 1234);
    case IPAddressSpace::kPublic:
      return webrtc::SocketAddress("8.8.8.8", 1234);
    case IPAddressSpace::kUnknown:
      NOTREACHED();
  }
}

}  // namespace

class PeerConnectionDependencyFactoryTest : public ::testing::Test {
 public:
  PeerConnectionDependencyFactoryTest()
      : mock_client_(
            MakeGarbageCollected<MockRTCPeerConnectionHandlerClient>()) {}
  void EnsureDependencyFactory(ExecutionContext& context) {
    dependency_factory_ = &PeerConnectionDependencyFactory::From(context);
    ASSERT_TRUE(dependency_factory_);
  }

  std::unique_ptr<RTCPeerConnectionHandler> CreateRTCPeerConnectionHandler() {
    std::unique_ptr<RTCPeerConnectionHandler> handler =
        dependency_factory_->CreateRTCPeerConnectionHandler(
            mock_client_.Get(),
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*encoded_insertable_streams=*/false);
    DummyExceptionStateForTesting exception_state;
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    handler->InitializeForTest(config,
                               /*peer_connection_tracker=*/nullptr,
                               exception_state,
                               /*rtp_transport=*/nullptr);
    return handler;
  }

 protected:
  test::TaskEnvironment task_environment_;
  Persistent<PeerConnectionDependencyFactory> dependency_factory_;
  Persistent<MockRTCPeerConnectionHandlerClient> mock_client_;
};

TEST_F(PeerConnectionDependencyFactoryTest, CreateRTCPeerConnectionHandler) {
  V8TestingScope scope;
  EnsureDependencyFactory(*scope.GetExecutionContext());

  std::unique_ptr<RTCPeerConnectionHandler> pc_handler =
      CreateRTCPeerConnectionHandler();
  EXPECT_TRUE(pc_handler);
}

struct TestCase {
  IPAddressSpace originator_address_space;
  IPAddressSpace candidate_address_space;
  bool should_request_permission;
};

class LocalNetworkAccessPeerConnectionDependencyFactoryTest
    : public PeerConnectionDependencyFactoryTest,
      public testing::WithParamInterface<TestCase> {};

constexpr TestCase kTestCases[] = {
    {.originator_address_space = IPAddressSpace::kLoopback,
     .candidate_address_space = IPAddressSpace::kLoopback,
     .should_request_permission = false},
    {.originator_address_space = IPAddressSpace::kLoopback,
     .candidate_address_space = IPAddressSpace::kLocal,
     .should_request_permission = false},
    {.originator_address_space = IPAddressSpace::kLoopback,
     .candidate_address_space = IPAddressSpace::kPublic,
     .should_request_permission = false},
    {.originator_address_space = IPAddressSpace::kLocal,
     .candidate_address_space = IPAddressSpace::kLoopback,
     .should_request_permission = true},
    {.originator_address_space = IPAddressSpace::kLocal,
     .candidate_address_space = IPAddressSpace::kLocal,
     .should_request_permission = false},
    {.originator_address_space = IPAddressSpace::kLocal,
     .candidate_address_space = IPAddressSpace::kPublic,
     .should_request_permission = false},
    {.originator_address_space = IPAddressSpace::kPublic,
     .candidate_address_space = IPAddressSpace::kLoopback,
     .should_request_permission = true},
    {.originator_address_space = IPAddressSpace::kPublic,
     .candidate_address_space = IPAddressSpace::kLocal,
     .should_request_permission = true},
    {.originator_address_space = IPAddressSpace::kPublic,
     .candidate_address_space = IPAddressSpace::kPublic,
     .should_request_permission = false},
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LocalNetworkAccessPeerConnectionDependencyFactoryTest,
    testing::ValuesIn(kTestCases),
    [](const testing::TestParamInfo<TestCase>& info) {
      return base::StrCat({base::ToString(info.param.originator_address_space),
                           "_",
                           base::ToString(info.param.candidate_address_space)});
    });

TEST_P(LocalNetworkAccessPeerConnectionDependencyFactoryTest,
       ShouldRequestPermission) {
  const auto [originator_address_space, candidate_address_space,
              should_request_permission] = GetParam();

  WebRuntimeFeatures::EnableLocalNetworkAccessWebRTC(true);

  V8TestingScope scope;
  auto& dependency_factory =
      PeerConnectionDependencyFactory::From(*scope.GetExecutionContext());

  auto policies = mojom::blink::PolicyContainerPolicies::New();
  policies->ip_address_space = originator_address_space;
  auto policy_container = std::make_unique<PolicyContainer>(
      mojo::NullAssociatedRemote(), std::move(policies));

  scope.GetExecutionContext()->SetPolicyContainer(std::move(policy_container));

  auto lna_permission_factory =
      dependency_factory.CreateLocalNetworkAccessPermissionFactoryForTesting();
  auto lna_permission = lna_permission_factory->Create();

  EXPECT_EQ(should_request_permission,
            lna_permission->ShouldRequestPermission(
                AddressForSpace(candidate_address_space)));
}

}  // namespace blink
