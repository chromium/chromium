// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_storage_worklet_devtools_agent_host.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"
#include "content/browser/shared_storage/shared_storage_worklet_host.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class TestDevToolsAgentHostClient : public DevToolsAgentHostClient {
 public:
  TestDevToolsAgentHostClient() : closed_(false) {}

  TestDevToolsAgentHostClient(const TestDevToolsAgentHostClient&) = delete;
  TestDevToolsAgentHostClient& operator=(const TestDevToolsAgentHostClient&) =
      delete;

  ~TestDevToolsAgentHostClient() override { EXPECT_TRUE(closed_); }

  void Close() {
    EXPECT_FALSE(closed_);
    agent_host_->DetachClient(this);
    closed_ = true;
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override { FAIL(); }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    last_message_ = std::string(reinterpret_cast<const char*>(message.data()),
                                message.size());
  }

  void InspectAgentHost(DevToolsAgentHost* agent_host) {
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);
  }

  DevToolsAgentHost* agent_host() { return agent_host_.get(); }

  const std::string& last_message() const { return last_message_; }

 private:
  std::string last_message_;

  bool closed_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
};

class MockContentBrowserClient : public ContentBrowserClient {
 public:
  MockContentBrowserClient() = default;
  ~MockContentBrowserClient() override = default;

  bool IsSharedStorageAllowed(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* rfh,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message,
      bool* out_block_is_site_setting_specific) override {
    return true;
  }
};

}  // namespace

class SharedStorageWorkletDevToolsAgentHostTest
    : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    mock_content_browser_client_ = std::make_unique<MockContentBrowserClient>();
    original_client_ =
        SetBrowserClientForTesting(mock_content_browser_client_.get());
    DevToolsManager::ShutdownForTests();

    contents()->NavigateAndCommit(GURL("https://www.google.com"));
    RenderFrameHost* main_rfh = web_contents()->GetPrimaryMainFrame();

    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host;

    GURL script_url("https://www.google.com/script.js");

    SharedStorageDocumentServiceImpl* document_service =
        SharedStorageDocumentServiceImpl::GetOrCreateForCurrentDocument(
            main_rfh);
    document_service->CreateWorklet(
        script_url, url::Origin::Create(script_url),
        network::mojom::CredentialsMode::kSameOrigin, {},
        std::move(worklet_host), base::DoNothing());

    SharedStorageWorkletHostManager* manager =
        GetSharedStorageWorkletHostManagerForStoragePartition(
            main_rfh->GetStoragePartition());
    std::map<SharedStorageDocumentServiceImpl*,
             std::map<SharedStorageWorkletHost*,
                      std::unique_ptr<SharedStorageWorkletHost>>>&
        worklet_hosts = manager->GetAttachedWorkletHostsForTesting();
    CHECK_EQ(worklet_hosts.size(), 1u);
    CHECK_EQ((worklet_hosts.begin()->second).size(), 1u);

    worklet_host_ = worklet_hosts.begin()->second.begin()->second.get();

    devtools_agent_host_ = new SharedStorageWorkletDevToolsAgentHost(
        *worklet_host_, base::UnguessableToken());
  }
  void TearDown() override {
    SetBrowserClientForTesting(original_client_);
    DevToolsManager::ShutdownForTests();
    devtools_agent_host_.reset();
    worklet_host_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  raw_ptr<SharedStorageWorkletHost> worklet_host_;
  scoped_refptr<SharedStorageWorkletDevToolsAgentHost> devtools_agent_host_;
  std::unique_ptr<ContentBrowserClient> mock_content_browser_client_;
  raw_ptr<ContentBrowserClient> original_client_ = nullptr;
};

TEST_F(SharedStorageWorkletDevToolsAgentHostTest, BasicAttributes) {
  EXPECT_EQ(devtools_agent_host_->GetBrowserContext(),
            web_contents()->GetBrowserContext());
  EXPECT_EQ(devtools_agent_host_->GetType(), "shared_storage_worklet");
  EXPECT_EQ(devtools_agent_host_->GetTitle(),
            "Shared storage worklet for https://www.google.com/script.js");
  EXPECT_EQ(devtools_agent_host_->GetURL(),
            GURL("https://www.google.com/script.js"));
  EXPECT_FALSE(devtools_agent_host_->Activate());
  EXPECT_FALSE(devtools_agent_host_->Close());

  devtools_agent_host_->WorkletDestroyed();
  EXPECT_EQ(devtools_agent_host_->GetBrowserContext(), nullptr);
  EXPECT_EQ(devtools_agent_host_->GetType(), "shared_storage_worklet");
  EXPECT_EQ(devtools_agent_host_->GetTitle(), std::string());
  EXPECT_EQ(devtools_agent_host_->GetURL(), GURL());
}

TEST_F(SharedStorageWorkletDevToolsAgentHostTest,
       OpenAndManuallyCloseDevToolsClientHost) {
  EXPECT_FALSE(devtools_agent_host_->IsAttached());

  TestDevToolsAgentHostClient host_client;
  host_client.InspectAgentHost(devtools_agent_host_.get());
  // Test that the connection is established.
  EXPECT_TRUE(devtools_agent_host_->IsAttached());

  host_client.Close();
  EXPECT_FALSE(devtools_agent_host_->IsAttached());
}

TEST_F(SharedStorageWorkletDevToolsAgentHostTest, WorkletDestroyed) {
  TestDevToolsAgentHostClient host_client;
  host_client.InspectAgentHost(devtools_agent_host_.get());

  devtools_agent_host_->WorkletDestroyed();
  EXPECT_EQ(host_client.last_message(),
            "{\"method\":\"Inspector.targetCrashed\",\"params\":{}}");

  host_client.Close();
}

// Regression test for crbug.com/1515243.
TEST_F(SharedStorageWorkletDevToolsAgentHostTest,
       CheckIsRelevantToFrame_WorkletHostDetachedFromDocument) {
  // The test assumes that the page gets deleted after navigation. Disable
  // back/forward cache to ensure that pages don't get preserved in the cache.
  DisableBackForwardCacheForTesting(web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // Navigate to a new page. The worklet will no longer be associated with a
  // document.
  contents()->NavigateAndCommit(GURL("https://www.youtube.com"));

  EXPECT_FALSE(
      devtools_agent_host_->IsRelevantTo(static_cast<RenderFrameHostImpl*>(
          web_contents()->GetPrimaryMainFrame())));
}

}  // namespace content
