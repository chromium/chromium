// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/input/input_constants.h"
#include "components/input/render_input_router.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestDevToolsClientHost : public DevToolsAgentHostClient {
 public:
  TestDevToolsClientHost() : closed_(false) {}

  TestDevToolsClientHost(const TestDevToolsClientHost&) = delete;
  TestDevToolsClientHost& operator=(const TestDevToolsClientHost&) = delete;

  ~TestDevToolsClientHost() override { EXPECT_TRUE(closed_); }

  void Close() {
    EXPECT_FALSE(closed_);
    close_counter++;
    agent_host_->DetachClient(this);
    closed_ = true;
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override { FAIL(); }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}

  void InspectAgentHost(DevToolsAgentHost* agent_host) {
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);
  }

  DevToolsAgentHost* agent_host() { return agent_host_.get(); }

  static void ResetCounters() { close_counter = 0; }

  static int close_counter;

 private:
  bool closed_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

int TestDevToolsClientHost::close_counter = 0;

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() : renderer_unresponsive_received_(false) {}

  // Notification that the contents is hung.
  void RendererUnresponsive(
      WebContents* source,
      RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override {
    renderer_unresponsive_received_ = true;
  }

  bool renderer_unresponsive_received() const {
    return renderer_unresponsive_received_;
  }

 private:
  bool renderer_unresponsive_received_;
};

class RecordingDevToolsManagerDelegate : public DevToolsManagerDelegate {
 public:
  static RecordingDevToolsManagerDelegate* last_instance;
  RecordingDevToolsManagerDelegate() { last_instance = this; }
  ~RecordingDevToolsManagerDelegate() override {
    if (last_instance == this) {
      last_instance = nullptr;
    }
  }

  void ClientAttached(DevToolsAgentHostClientChannel* channel) override {
    last_attached_client_ = channel->GetClient();
    last_attached_host_ = channel->GetAgentHost();
  }

  void ClientDetached(DevToolsAgentHostClientChannel* channel) override {
    if (last_attached_client_ == channel->GetClient()) {
      last_attached_client_ = nullptr;
    }
    if (last_attached_host_ == channel->GetAgentHost()) {
      last_attached_host_ = nullptr;
    }
  }

  DevToolsAgentHostClient* last_attached_client() const {
    return last_attached_client_;
  }
  DevToolsAgentHost* last_attached_host() const { return last_attached_host_; }

 private:
  raw_ptr<DevToolsAgentHostClient> last_attached_client_ = nullptr;
  raw_ptr<DevToolsAgentHost> last_attached_host_ = nullptr;
};

RecordingDevToolsManagerDelegate*
    RecordingDevToolsManagerDelegate::last_instance = nullptr;

class BrowserClient : public ContentBrowserClient {
 public:
  BrowserClient() = default;
  ~BrowserClient() override = default;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override {
    return std::make_unique<RecordingDevToolsManagerDelegate>();
  }
};

}  // namespace

class DevToolsAgentHostImplTest : public RenderViewHostImplTestHarness {
 public:
  DevToolsAgentHostImplTest() {}

 protected:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    TestDevToolsClientHost::ResetCounters();
    browser_content_client_ = std::make_unique<BrowserClient>();
    original_client_ =
        SetBrowserClientForTesting(browser_content_client_.get());
    DevToolsManager::ShutdownForTests();
  }
  void TearDown() override {
    SetBrowserClientForTesting(original_client_);
    DevToolsManager::ShutdownForTests();

    RenderViewHostImplTestHarness::TearDown();
  }

 private:
  std::unique_ptr<ContentBrowserClient> browser_content_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
};

TEST_F(DevToolsAgentHostImplTest, OpenAndManuallyCloseDevToolsClientHost) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(web_contents()));
  EXPECT_FALSE(agent->IsAttached());

  TestDevToolsClientHost client_host;
  client_host.InspectAgentHost(agent.get());
  // Test that the connection is established.
  EXPECT_TRUE(agent->IsAttached());
  EXPECT_EQ(0, TestDevToolsClientHost::close_counter);

  client_host.Close();
  EXPECT_EQ(1, TestDevToolsClientHost::close_counter);
  EXPECT_FALSE(agent->IsAttached());
}

TEST_F(DevToolsAgentHostImplTest, NoUnresponsiveDialogInInspectedContents) {
  const GURL url("http://www.google.com");
  contents()->NavigateAndCommit(url);
  TestRenderViewHost* inspected_rvh = test_rvh();
  EXPECT_TRUE(inspected_rvh->IsRenderViewLive());
  EXPECT_FALSE(contents()->GetDelegate());
  TestWebContentsDelegate delegate;
  contents()->SetDelegate(&delegate);

  TestDevToolsClientHost client_host;
  scoped_refptr<DevToolsAgentHost> agent_host(DevToolsAgentHost::GetOrCreateFor(
      WebContents::FromRenderViewHost(inspected_rvh)));
  client_host.InspectAgentHost(agent_host.get());

  // Start a timeout.
  inspected_rvh->GetWidget()
      ->GetRenderInputRouter()
      ->StartInputEventAckTimeoutForTesting();
  task_environment()->FastForwardBy(input::kHungRendererDelay +
                                    base::Milliseconds(10));
  EXPECT_FALSE(delegate.renderer_unresponsive_received());

  // Now close devtools and check that the notification is delivered.
  client_host.Close();
  // Start a timeout.
  inspected_rvh->GetWidget()
      ->GetRenderInputRouter()
      ->StartInputEventAckTimeoutForTesting();
  task_environment()->FastForwardBy(input::kHungRendererDelay +
                                    base::Milliseconds(10));
  EXPECT_TRUE(delegate.renderer_unresponsive_received());

  contents()->SetDelegate(nullptr);
}

class TestExternalAgentDelegate : public DevToolsExternalAgentProxyDelegate {
 public:
  TestExternalAgentDelegate() {}
  ~TestExternalAgentDelegate() override {
    expectEvent(1, "Attach");
    expectEvent(1, "Detach");
    expectEvent(0, "SendMessageToBackend.message0");
    expectEvent(1, "SendMessageToBackend.message1");
    expectEvent(2, "SendMessageToBackend.message2");
  }

 private:
  std::map<std::string, int> event_counter_;

  void recordEvent(const std::string& name) {
    if (!event_counter_.contains(name)) {
      event_counter_[name] = 0;
    }
    event_counter_[name] = event_counter_[name] + 1;
  }

  void expectEvent(int count, const std::string& name) {
    EXPECT_EQ(count, event_counter_[name]);
  }

  void Attach(DevToolsExternalAgentProxy* proxy) override {
    recordEvent("Attach");
  }

  void Detach(DevToolsExternalAgentProxy* proxy) override {
    recordEvent("Detach");
  }

  std::string GetType() override { return std::string(); }
  std::string GetTitle() override { return std::string(); }
  std::string GetDescription() override { return std::string(); }
  GURL GetURL() override { return GURL(); }
  GURL GetFaviconURL() override { return GURL(); }
  std::string GetFrontendURL() override { return std::string(); }
  bool Activate() override { return false; }
  void Reload() override {}
  bool Close() override { return false; }
  base::TimeTicks GetLastActivityTime() override { return base::TimeTicks(); }

  void SendMessageToBackend(DevToolsExternalAgentProxy* proxy,
                            base::span<const uint8_t> message) override {
    recordEvent(std::string("SendMessageToBackend.") +
                std::string(message.begin(), message.end()));
  }
};

TEST_F(DevToolsAgentHostImplTest, TestExternalProxy) {
  std::unique_ptr<TestExternalAgentDelegate> delegate(
      new TestExternalAgentDelegate());

  scoped_refptr<DevToolsAgentHost> agent_host = DevToolsAgentHost::Forward(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), std::move(delegate));
  EXPECT_EQ(agent_host, DevToolsAgentHost::GetForId(agent_host->GetId()));

  TestDevToolsClientHost client_host;
  client_host.InspectAgentHost(agent_host.get());

  agent_host->DispatchProtocolMessage(&client_host,
                                      base::byte_span_from_cstring("message1"));
  agent_host->DispatchProtocolMessage(&client_host,
                                      base::byte_span_from_cstring("message2"));
  agent_host->DispatchProtocolMessage(&client_host,
                                      base::byte_span_from_cstring("message2"));

  client_host.Close();
}

class CustomMetadataDevToolsClientHost : public TestDevToolsClientHost {
 public:
  explicit CustomMetadataDevToolsClientHost(PeerMetadata peer_metadata)
      : peer_metadata_(std::move(peer_metadata)) {}

  PeerMetadata GetPeerMetadata() override { return peer_metadata_; }

 private:
  PeerMetadata peer_metadata_;
};

TEST_F(DevToolsAgentHostImplTest,
       TargetHandlerSessionDelegatesMetadataViaAttachToTarget) {
  scoped_refptr<DevToolsAgentHost> page_host =
      DevToolsAgentHost::GetOrCreateFor(web_contents());
  scoped_refptr<DevToolsAgentHost> browser_host =
      DevToolsAgentHost::CreateForDiscovery();

  const net::IPEndPoint kPeerAddress(net::IPAddress::IPv4Localhost(), 54321);
  CustomMetadataDevToolsClientHost root_client(
      {.transport_type =
           DevToolsAgentHostClient::PeerMetadata::TransportType::kWebSocket,
       .peer_address = kPeerAddress});
  root_client.InspectAgentHost(browser_host.get());
  ASSERT_NE(RecordingDevToolsManagerDelegate::last_instance, nullptr);

  std::string attach_cmd = base::StringPrintf(
      "{\"id\":1,\"method\":\"Target.attachToTarget\","
      "\"params\":{\"targetId\":\"%s\",\"flatten\":true}}",
      page_host->GetId().c_str());
  browser_host->DispatchProtocolMessage(&root_client,
                                        base::as_byte_span(attach_cmd));

  EXPECT_EQ(
      RecordingDevToolsManagerDelegate::last_instance->last_attached_host(),
      page_host.get());
  DevToolsAgentHostClient* child_client =
      RecordingDevToolsManagerDelegate::last_instance->last_attached_client();
  ASSERT_NE(child_client, nullptr);
  EXPECT_NE(child_client, &root_client);
  DevToolsAgentHostClient::PeerMetadata child_metadata =
      child_client->GetPeerMetadata();
  EXPECT_EQ(child_metadata.transport_type,
            DevToolsAgentHostClient::PeerMetadata::TransportType::kWebSocket);
  EXPECT_EQ(child_metadata.peer_address, kPeerAddress);

  root_client.Close();
}

TEST_F(DevToolsAgentHostImplTest,
       TargetHandlerSessionDelegatesMetadataViaSetAutoAttach) {
  scoped_refptr<DevToolsAgentHost> page_host =
      DevToolsAgentHost::GetOrCreateFor(web_contents());
  scoped_refptr<DevToolsAgentHost> browser_host =
      DevToolsAgentHost::CreateForDiscovery();

  CustomMetadataDevToolsClientHost root_client(
      {.transport_type =
           DevToolsAgentHostClient::PeerMetadata::TransportType::kPipe,
       .peer_address = std::nullopt});
  root_client.InspectAgentHost(browser_host.get());
  ASSERT_NE(RecordingDevToolsManagerDelegate::last_instance, nullptr);

  constexpr char kAutoAttachCmd[] =
      "{\"id\":1,\"method\":\"Target.setAutoAttach\","
      "\"params\":{\"autoAttach\":true,\"waitForDebuggerOnStart\":false,"
      "\"flatten\":true}}";
  browser_host->DispatchProtocolMessage(
      &root_client, base::byte_span_from_cstring(kAutoAttachCmd));

  EXPECT_EQ(
      RecordingDevToolsManagerDelegate::last_instance->last_attached_host(),
      page_host.get());
  DevToolsAgentHostClient* child_client =
      RecordingDevToolsManagerDelegate::last_instance->last_attached_client();
  ASSERT_NE(child_client, nullptr);
  EXPECT_NE(child_client, &root_client);
  DevToolsAgentHostClient::PeerMetadata child_metadata =
      child_client->GetPeerMetadata();
  EXPECT_EQ(child_metadata.transport_type,
            DevToolsAgentHostClient::PeerMetadata::TransportType::kPipe);
  EXPECT_EQ(child_metadata.peer_address, std::nullopt);

  root_client.Close();
}

TEST_F(DevToolsAgentHostImplTest,
       TargetHandlerDelegatesMetadataViaExposeDevToolsProtocol) {
  scoped_refptr<DevToolsAgentHost> page_host =
      DevToolsAgentHost::GetOrCreateFor(web_contents());
  scoped_refptr<DevToolsAgentHost> browser_host =
      DevToolsAgentHost::CreateForDiscovery();

  const net::IPEndPoint kPeerAddress(net::IPAddress::IPv4Localhost(), 54321);
  CustomMetadataDevToolsClientHost root_client(
      {.transport_type =
           DevToolsAgentHostClient::PeerMetadata::TransportType::kWebSocket,
       .peer_address = kPeerAddress});
  root_client.InspectAgentHost(browser_host.get());
  ASSERT_NE(RecordingDevToolsManagerDelegate::last_instance, nullptr);

  std::string expose_cmd = base::StringPrintf(
      "{\"id\":1,\"method\":\"Target.exposeDevToolsProtocol\","
      "\"params\":{\"targetId\":\"%s\"}}",
      page_host->GetId().c_str());
  browser_host->DispatchProtocolMessage(&root_client,
                                        base::as_byte_span(expose_cmd));

  EXPECT_EQ(
      RecordingDevToolsManagerDelegate::last_instance->last_attached_host(),
      page_host.get());
  DevToolsAgentHostClient* connector_client =
      RecordingDevToolsManagerDelegate::last_instance->last_attached_client();
  ASSERT_NE(connector_client, nullptr);
  EXPECT_NE(connector_client, &root_client);
  DevToolsAgentHostClient::PeerMetadata connector_metadata =
      connector_client->GetPeerMetadata();
  EXPECT_EQ(connector_metadata.transport_type,
            DevToolsAgentHostClient::PeerMetadata::TransportType::kWebSocket);
  EXPECT_EQ(connector_metadata.peer_address, kPeerAddress);

  root_client.Close();
}

}  // namespace content
