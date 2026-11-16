// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/network_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content::protocol {
namespace {

constexpr std::string_view kThrottlingRule =
    R"({"matchedNetworkConditions": [{"urlPattern": "", "latency": 100,
        "downloadThroughput": -1, "uploadThroughput": -1}]})";

// Counts the network conditions that a network context is told to apply.
class ConditionsRecordingNetworkContext : public network::TestNetworkContext {
 public:
  mojo::PendingRemote<network::mojom::NetworkContext>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Flush() { receiver_.FlushForTesting(); }

  int set_conditions_calls() const { return set_conditions_calls_; }
  int set_socket_layer_conditions_calls() const {
    return set_socket_layer_conditions_calls_;
  }
  int clear_socket_layer_conditions_calls() const {
    return clear_socket_layer_conditions_calls_;
  }

  // network::mojom::NetworkContext:
  void SetNetworkConditions(
      const base::UnguessableToken& throttling_profile_id,
      const base::UnguessableToken& throttling_client_id,
      std::vector<network::mojom::MatchedNetworkConditionsPtr> conditions)
      override {
    ++set_conditions_calls_;
  }
  void SetSocketLayerNetworkConditions(
      const base::UnguessableToken& throttling_profile_id,
      const base::UnguessableToken& throttling_client_id,
      uint64_t client_order,
      base::TimeDelta latency,
      double download_throughput_bytes_per_second,
      double upload_throughput_bytes_per_second) override {
    ++set_socket_layer_conditions_calls_;
  }
  void ClearSocketLayerNetworkConditions(
      const base::UnguessableToken& throttling_profile_id,
      const base::UnguessableToken& throttling_client_id) override {
    ++clear_socket_layer_conditions_calls_;
  }

 private:
  mojo::Receiver<network::mojom::NetworkContext> receiver_{this};
  int set_conditions_calls_ = 0;
  int set_socket_layer_conditions_calls_ = 0;
  int clear_socket_layer_conditions_calls_ = 0;
};

// A storage partition, in a browser context of its own, whose network context
// records the network conditions it's told to apply.
class RecordingStoragePartition {
 public:
  RecordingStoragePartition()
      : browser_context_(std::make_unique<TestBrowserContext>()) {
    partition()->SetNetworkContextForTesting(
        network_context_.BindNewPipeAndPassRemote());
  }

  StoragePartitionImpl* partition() {
    return static_cast<StoragePartitionImpl*>(
        browser_context_->GetDefaultStoragePartition());
  }

  ConditionsRecordingNetworkContext& network_context() {
    network_context_.Flush();
    return network_context_;
  }

  // Destroys the partition, by destroying its browser context.
  void Destroy() { browser_context_.reset(); }

 private:
  ConditionsRecordingNetworkContext network_context_;
  std::unique_ptr<TestBrowserContext> browser_context_;
};

class TestDevToolsClient : public DevToolsAgentHostClient {
 public:
  const std::vector<std::string>& messages() const { return messages_; }

  // DevToolsAgentHostClient:
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    messages_.emplace_back(base::as_string_view(message));
  }
  void AgentHostClosed(DevToolsAgentHost* agent_host) override {}

 private:
  std::vector<std::string> messages_;
};

// Tests that a NetworkHandler copes with its storage partition being destroyed
// first. That happens when the handler of an installing service worker gets the
// worker's storage partition from ThrottleServiceWorkerMainScriptFetch(), which
// the tests simulate with SetStoragePartition(), and the worker's browser
// context is disposed of before the worker starts: the worker's DevTools host,
// and so the handler, outlive the partition.
class NetworkHandlerStoragePartitionTest
    : public RenderViewHostImplTestHarness {
 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    agent_host_ = DevToolsAgentHost::GetOrCreateFor(web_contents());
    ASSERT_TRUE(agent_host_->AttachClient(&client_));
    std::vector<NetworkHandler*> handlers = NetworkHandler::ForAgentHost(
        static_cast<DevToolsAgentHostImpl*>(agent_host_.get()));
    ASSERT_EQ(handlers.size(), 1u);
    handler_ = handlers.front();
  }

  void TearDown() override {
    if (handler_) {
      Detach();
    }
    agent_host_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  NetworkHandler& handler() { return *handler_; }

  // Sends a command, and returns whether it succeeded.
  bool SendCommand(std::string_view method, std::string_view params = "{}") {
    const int id = ++last_command_id_;
    const std::string command = base::StrCat(
        {R"({"id": )", base::NumberToString(id), R"(, "method": ")", method,
         R"(", "params": )", params, "}"});
    agent_host_->DispatchProtocolMessage(&client_, base::as_byte_span(command));
    for (const std::string& message : client_.messages()) {
      std::optional<base::DictValue> response =
          base::JSONReader::ReadDict(message, base::JSON_PARSE_RFC);
      if (response && response->FindInt("id") == id) {
        return !response->FindDict("error");
      }
    }
    ADD_FAILURE() << "No response to " << method;
    return false;
  }

  // Detaches the session, which disables its handlers.
  void Detach() {
    handler_ = nullptr;
    EXPECT_TRUE(agent_host_->DetachClient(&client_));
  }

 private:
  scoped_refptr<DevToolsAgentHost> agent_host_;
  TestDevToolsClient client_;
  raw_ptr<NetworkHandler> handler_ = nullptr;
  int last_command_id_ = 0;
};

TEST_F(NetworkHandlerStoragePartitionTest, OutlivesStoragePartition) {
  RecordingStoragePartition storage_partition;
  handler().SetStoragePartition(storage_partition.partition());
  ASSERT_TRUE(
      SendCommand("Network.emulateNetworkConditionsByRule", kThrottlingRule));
  EXPECT_EQ(storage_partition.network_context().set_conditions_calls(), 1);

  storage_partition.Destroy();

  // Commands that need the partition fail rather than use it, and so does
  // clearing the conditions when the session detaches.
  EXPECT_FALSE(SendCommand("Network.getCookies"));
  Detach();
}

TEST_F(NetworkHandlerStoragePartitionTest,
       OutlivesStoragePartitionWithSocketLayerThrottling) {
  base::test::ScopedFeatureList feature_list(
      network::features::kSocketLayerNetworkThrottling);
  RecordingStoragePartition storage_partition;
  handler().SetStoragePartition(storage_partition.partition());
  ASSERT_TRUE(
      SendCommand("Network.emulateNetworkConditionsByRule", kThrottlingRule));
  ConditionsRecordingNetworkContext& network_context =
      storage_partition.network_context();
  EXPECT_EQ(network_context.set_socket_layer_conditions_calls(), 1);
  // Offline emulation still applies at the request level.
  EXPECT_EQ(network_context.set_conditions_calls(), 1);

  storage_partition.Destroy();

  // Detaching clears the socket-layer conditions from where they were applied,
  // which went away with the partition.
  Detach();
}

// Socket-layer conditions move with the handler to another storage partition,
// and are cleared from the one they leave, and from the one they're in when the
// session detaches.
TEST_F(NetworkHandlerStoragePartitionTest,
       SocketLayerConditionsMoveToAnotherStoragePartition) {
  base::test::ScopedFeatureList feature_list(
      network::features::kSocketLayerNetworkThrottling);
  RecordingStoragePartition first_partition;
  RecordingStoragePartition second_partition;
  handler().SetStoragePartition(first_partition.partition());
  ASSERT_TRUE(
      SendCommand("Network.emulateNetworkConditionsByRule", kThrottlingRule));
  EXPECT_EQ(
      first_partition.network_context().set_socket_layer_conditions_calls(), 1);

  handler().SetStoragePartition(second_partition.partition());
  EXPECT_EQ(
      first_partition.network_context().clear_socket_layer_conditions_calls(),
      1);
  EXPECT_EQ(
      second_partition.network_context().set_socket_layer_conditions_calls(),
      1);

  Detach();
  EXPECT_EQ(
      second_partition.network_context().clear_socket_layer_conditions_calls(),
      1);
}

// When the storage partition that socket-layer conditions apply to is
// destroyed, they still move with the handler to another one.
TEST_F(NetworkHandlerStoragePartitionTest,
       SocketLayerConditionsMoveFromDestroyedStoragePartition) {
  base::test::ScopedFeatureList feature_list(
      network::features::kSocketLayerNetworkThrottling);
  RecordingStoragePartition first_partition;
  RecordingStoragePartition second_partition;
  handler().SetStoragePartition(first_partition.partition());
  ASSERT_TRUE(
      SendCommand("Network.emulateNetworkConditionsByRule", kThrottlingRule));
  EXPECT_EQ(
      first_partition.network_context().set_socket_layer_conditions_calls(), 1);

  first_partition.Destroy();
  handler().SetStoragePartition(second_partition.partition());
  EXPECT_EQ(
      second_partition.network_context().set_socket_layer_conditions_calls(),
      1);

  Detach();
  EXPECT_EQ(
      second_partition.network_context().clear_socket_layer_conditions_calls(),
      1);
}

}  // namespace
}  // namespace content::protocol
