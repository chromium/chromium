// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_agent_host.h"

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/shared_worker_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class SharedWorkerDevToolsAgentHostTest : public testing::Test {
 protected:
  void SetUp() override {
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        &mock_render_process_host_factory_);
  }

  void TearDown() override {
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
  }

  BrowserTaskEnvironment task_environment_;
  MockRenderProcessHostFactory mock_render_process_host_factory_;
};

// Verifies that SharedWorkerDevToolsAgentHost avoids matching a host from one
// BrowserContext to a SharedWorkerHost running in a different BrowserContext.
TEST_F(SharedWorkerDevToolsAgentHostTest, AvoidCrossProfileMatches) {
  TestBrowserContext regular_context;
  TestBrowserContext incognito_context;

  const GURL kWorkerUrl("https://example.com/worker.js");
  blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(kWorkerUrl));

  // Setup First Context & SharedWorkerHost
  auto site_instance1 =
      SiteInstanceImpl::CreateForTesting(&regular_context, kWorkerUrl);
  RenderProcessHost* rph1 = site_instance1->GetOrCreateProcessForTesting();
  ASSERT_TRUE(rph1->Init());

  SharedWorkerServiceImpl service1(nullptr, nullptr);
  SharedWorkerInstance instance1(
      kWorkerUrl, blink::mojom::ScriptType::kClassic,
      network::mojom::CredentialsMode::kSameOrigin, "name", storage_key,
      storage_key, url::Origin::Create(kWorkerUrl),
      blink::mojom::SharedWorkerCreationContextType::kSecure,
      blink::mojom::SharedWorkerSameSiteCookies::kAll, false);

  auto worker_host1 = std::make_unique<SharedWorkerHost>(
      &service1, instance1, site_instance1,
      std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      base::MakeRefCounted<PolicyContainerHost>());

  // Create Agent Host
  auto agent_host = base::MakeRefCounted<SharedWorkerDevToolsAgentHost>(
      worker_host1.get(), base::UnguessableToken::Create());

  EXPECT_TRUE(agent_host->Matches(worker_host1.get()));

  // Setup Second Context & SharedWorkerHost (e.g. Incognito)
  auto site_instance2 =
      SiteInstanceImpl::CreateForTesting(&incognito_context, kWorkerUrl);
  RenderProcessHost* rph2 = site_instance2->GetOrCreateProcessForTesting();
  ASSERT_TRUE(rph2->Init());

  SharedWorkerServiceImpl service2(nullptr, nullptr);
  SharedWorkerInstance instance2(
      kWorkerUrl, blink::mojom::ScriptType::kClassic,
      network::mojom::CredentialsMode::kSameOrigin, "name", storage_key,
      storage_key, url::Origin::Create(kWorkerUrl),
      blink::mojom::SharedWorkerCreationContextType::kSecure,
      blink::mojom::SharedWorkerSameSiteCookies::kAll, false);

  auto worker_host2 = std::make_unique<SharedWorkerHost>(
      &service2, instance2, site_instance2,
      std::vector<network::mojom::ContentSecurityPolicyPtr>(),
      base::MakeRefCounted<PolicyContainerHost>());

  // Enforce that the agent host does not match a worker from another context,
  // even if they share identity.
  EXPECT_FALSE(agent_host->Matches(worker_host2.get()));
}

}  // namespace content
