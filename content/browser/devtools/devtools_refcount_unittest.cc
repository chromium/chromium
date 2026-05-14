// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "content/browser/devtools/worker_or_worklet_devtools_agent_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class TestAgentHost : public WorkerOrWorkletDevToolsAgentHost {
 public:
  TestAgentHost(
      int process_id,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      const std::string& parent_id,
      base::OnceCallback<void(DevToolsAgentHostImpl*)> destroyed_callback)
      : WorkerOrWorkletDevToolsAgentHost(process_id,
                                         url,
                                         name,
                                         devtools_worker_token,
                                         parent_id,
                                         std::move(destroyed_callback)) {
    NotifyCreated();
  }

  using WorkerOrWorkletDevToolsAgentHost::Disconnected;

  // DevToolsAgentHost implementation:
  std::string GetType() override { return "test"; }
  bool AttachSession(DevToolsSession* session) override { return true; }
  void DetachSession(DevToolsSession* session) override {}

 private:
  ~TestAgentHost() override = default;
};

}  // namespace

class DevToolsRefcountTest : public testing::Test {
 public:
  DevToolsRefcountTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
};

// Tests that WorkerOrWorkletDevToolsAgentHost doesn't underflow refcount
// when Disconnected() is called multiple times.
TEST_F(DevToolsRefcountTest, PreventDoubleRelease) {
  MockRenderProcessHost renderer_host(&browser_context_);
  const int process_id = renderer_host.GetDeprecatedID();
  base::UnguessableToken token = base::UnguessableToken::Create();

  bool destroyed_called = false;
  auto destroyed_callback = base::BindLambdaForTesting(
      [&](DevToolsAgentHostImpl* host) { destroyed_called = true; });

  // 1. Create a TestAgentHost.
  // It starts with refcount 1 (from our scoped_refptr) + 1 (from
  // self-keep-alive in constructor).
  scoped_refptr<TestAgentHost> agent_host = base::MakeRefCounted<TestAgentHost>(
      process_id, GURL("http://example.com"), "worker", token, "parent_id",
      std::move(destroyed_callback));

  // 2. Call Disconnected() first time.
  // This should trigger the first Release() and destroyed_callback.
  agent_host->Disconnected();
  EXPECT_TRUE(destroyed_called);

  // 3. Call Disconnected() multiple times.
  // If the fix is working, these subsequent calls won't cause double-release.
  // We reset destroyed_called to ensure it's not called again.
  destroyed_called = false;
  agent_host->Disconnected();
  agent_host->Disconnected();
  EXPECT_FALSE(destroyed_called);

  // 4. Verification: If we can still access agent_host, it hasn't been
  // over-released. The refcount should be 1 (only our scoped_refptr).
  EXPECT_TRUE(agent_host->HasOneRef());
}

}  // namespace content
