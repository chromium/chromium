// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"

#include <memory>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/task/current_thread.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
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
      return webrtc::SocketAddress("unresolved-hostname", 1234);
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
                               exception_state);
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

struct ShouldRequestPermissionTestCase {
  IPAddressSpace originator_address_space;
  IPAddressSpace candidate_address_space;
  bool result;
  // Result of ShouldRequestPermission() when the
  // kLocalNetworkAccessChecksWebRTCLoopbackOnly feature param is true.
  bool result_when_loopback_only;
  LocalNetworkAccessRequestType request_type;
};

class LocalNetworkAccessPeerConnectionDependencyFactoryTest
    : public PeerConnectionDependencyFactoryTest,
      public testing::WithParamInterface<
          std::tuple<bool, ShouldRequestPermissionTestCase>> {
 public:
  LocalNetworkAccessPeerConnectionDependencyFactoryTest() {
    const bool loopback_only = std::get<0>(GetParam());

    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{network::features::kLocalNetworkAccessChecksWebRTC,
          {{network::features::kLocalNetworkAccessChecksWebRTCLoopbackOnly.name,
            loopback_only ? "true" : "false"}}}},
        /*disabled_features=*/{});
  }

 protected:
  void TestUseCounters(Document& document,
                       LocalNetworkAccessRequestType request_type) {
    base::test::RunUntil([&]() {
      return document.IsUseCounted(
          mojom::blink::WebFeature::kWebRTCLocalNetworkAccessCheck);
    });

    EXPECT_EQ(
        request_type == LocalNetworkAccessRequestType::kPublicToLocal,
        document.IsUseCounted(
            mojom::blink::WebFeature::kWebRTCLocalNetworkAccessPublicToLocal));
    EXPECT_EQ(
        request_type == LocalNetworkAccessRequestType::kPublicToLoopback,
        document.IsUseCounted(mojom::blink::WebFeature::
                                  kWebRTCLocalNetworkAccessPublicToLoopback));
    EXPECT_EQ(
        request_type == LocalNetworkAccessRequestType::kLocalToLoopback,
        document.IsUseCounted(mojom::blink::WebFeature::
                                  kWebRTCLocalNetworkAccessLocalToLoopback));
  }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
};

constexpr ShouldRequestPermissionTestCase kTestCases[] = {
    {.originator_address_space = IPAddressSpace::kLoopback,
     .candidate_address_space = IPAddressSpace::kLoopback,
     .result = false,
     .result_when_loopback_only = false,
     .request_type = LocalNetworkAccessRequestType::kLoopbackToLoopback},
    {.originator_address_space = IPAddressSpace::kLoopback,
     .candidate_address_space = IPAddressSpace::kLocal,
     .result = false,
     .result_when_loopback_only = false,
     .request_type = LocalNetworkAccessRequestType::kLoopbackToLocal},
    {.originator_address_space = IPAddressSpace::kLoopback,
     .candidate_address_space = IPAddressSpace::kPublic,
     .result = false,
     .result_when_loopback_only = false,
     .request_type = LocalNetworkAccessRequestType::kLoopbackToPublic},
    {.originator_address_space = IPAddressSpace::kLocal,
     .candidate_address_space = IPAddressSpace::kLoopback,
     .result = true,
     .result_when_loopback_only = true,
     .request_type = LocalNetworkAccessRequestType::kLocalToLoopback},
    {.originator_address_space = IPAddressSpace::kLocal,
     .candidate_address_space = IPAddressSpace::kLocal,
     .result = false,
     .result_when_loopback_only = false,
     .request_type = LocalNetworkAccessRequestType::kLocalToLocal},
    {.originator_address_space = IPAddressSpace::kLocal,
     .candidate_address_space = IPAddressSpace::kPublic,
     .result = false,
     .result_when_loopback_only = false,
     .request_type = LocalNetworkAccessRequestType::kLocalToPublic},
    {.originator_address_space = IPAddressSpace::kPublic,
     .candidate_address_space = IPAddressSpace::kLoopback,
     .result = true,
     .result_when_loopback_only = true,
     .request_type = LocalNetworkAccessRequestType::kPublicToLoopback},
    {.originator_address_space = IPAddressSpace::kPublic,
     .candidate_address_space = IPAddressSpace::kLocal,
     .result = true,
     .result_when_loopback_only = false,
     .request_type = LocalNetworkAccessRequestType::kPublicToLocal},
    {.originator_address_space = IPAddressSpace::kPublic,
     .candidate_address_space = IPAddressSpace::kPublic,
     .result = false,
     .result_when_loopback_only = false,
     .request_type = LocalNetworkAccessRequestType::kPublicToPublic},
    {.originator_address_space = IPAddressSpace::kUnknown,
     .candidate_address_space = IPAddressSpace::kPublic,
     .result = false,
     .result_when_loopback_only = false,
     .request_type = LocalNetworkAccessRequestType::kUnknown},
    {.originator_address_space = IPAddressSpace::kPublic,
     .candidate_address_space = IPAddressSpace::kUnknown,
     .result = false,
     .result_when_loopback_only = false,
     .request_type = LocalNetworkAccessRequestType::kUnknown},
};

std::string PrintTestName(
    const testing::TestParamInfo<
        std::tuple<bool, ShouldRequestPermissionTestCase>>& info) {
  const bool loopback_only = std::get<0>(info.param);
  const auto& [originator_address_space, candidate_address_space, unused1,
               unused2, unused3] = std::get<1>(info.param);
  return base::StrCat({loopback_only ? "LoopbackOnly_" : "",
                       base::ToString(originator_address_space), "_",
                       base::ToString(candidate_address_space)});
}

INSTANTIATE_TEST_SUITE_P(,
                         LocalNetworkAccessPeerConnectionDependencyFactoryTest,
                         testing::Combine(testing::Values(false, true),
                                          testing::ValuesIn(kTestCases)),
                         &PrintTestName);

TEST_P(LocalNetworkAccessPeerConnectionDependencyFactoryTest,
       ShouldRequestPermission) {
  const auto [originator_address_space, candidate_address_space, result,
              result_with_loopback_only, request_type] =
      std::get<1>(GetParam());

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

  const bool loopback_only_enabled = std::get<0>(GetParam());
  const bool expected =
      loopback_only_enabled ? result_with_loopback_only : result;

  EXPECT_EQ(expected, lna_permission->ShouldRequestPermission(
                          AddressForSpace(candidate_address_space)));

  histogram_tester_.ExpectUniqueSample(
      "WebRTC.PeerConnection.LocalNetworkAccess.RequestType", request_type, 1);
  TestUseCounters(scope.GetDocument(), request_type);
}

TEST_P(LocalNetworkAccessPeerConnectionDependencyFactoryTest,
       ShouldRequestPermission_FeatureDisabled) {
  const auto [originator_address_space, candidate_address_space, result,
              result_with_loopback_only, request_type] =
      std::get<1>(GetParam());

  WebRuntimeFeatures::EnableLocalNetworkAccessWebRTC(false);

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

  EXPECT_FALSE(lna_permission->ShouldRequestPermission(
      AddressForSpace(candidate_address_space)));

  histogram_tester_.ExpectUniqueSample(
      "WebRTC.PeerConnection.LocalNetworkAccess.RequestType", request_type, 1);
  TestUseCounters(scope.GetDocument(), request_type);
}

}  // namespace blink
