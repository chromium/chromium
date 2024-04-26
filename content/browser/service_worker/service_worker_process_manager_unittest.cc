// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/url_schemes.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_client.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

using AncestorFrameType = blink::mojom::AncestorFrameType;

namespace {

// Keeps track of the most recent |site_instance| passed to
// CreateRenderProcessHost().
class SiteInstanceRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  SiteInstanceRenderProcessHostFactory() = default;

  SiteInstanceRenderProcessHostFactory(
      const SiteInstanceRenderProcessHostFactory&) = delete;
  SiteInstanceRenderProcessHostFactory& operator=(
      const SiteInstanceRenderProcessHostFactory&) = delete;

  ~SiteInstanceRenderProcessHostFactory() override = default;

  RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstance* site_instance) override {
    processes_.push_back(std::make_unique<MockRenderProcessHost>(
        browser_context, site_instance->GetStoragePartitionConfig(),
        site_instance->IsGuest()));

    // A spare RenderProcessHost is created with a null SiteInstance.
    if (site_instance)
      last_site_instance_used_ = site_instance;

    return processes_.back().get();
  }

  SiteInstance* last_site_instance_used() {
    return last_site_instance_used_.get();
  }

 private:
  std::vector<std::unique_ptr<MockRenderProcessHost>> processes_;
  scoped_refptr<SiteInstance> last_site_instance_used_;
};

}  // namespace

class ServiceWorkerProcessManagerTest : public testing::Test {
 public:
  ServiceWorkerProcessManagerTest() = default;

  ServiceWorkerProcessManagerTest(const ServiceWorkerProcessManagerTest&) =
      delete;
  ServiceWorkerProcessManagerTest& operator=(
      const ServiceWorkerProcessManagerTest&) = delete;

  void SetUp() override {
    browser_context_ = std::make_unique<TestBrowserContext>();
    process_manager_ = std::make_unique<ServiceWorkerProcessManager>();
    process_manager_->set_storage_partition(static_cast<StoragePartitionImpl*>(
        browser_context_->GetDefaultStoragePartition()));
    script_url_ = GURL("http://www.example.com/sw.js");
    render_process_host_factory_ =
        std::make_unique<SiteInstanceRenderProcessHostFactory>();
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        render_process_host_factory_.get());
  }

  void TearDown() override {
    process_manager_->Shutdown();
    process_manager_.reset();
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(nullptr);
    render_process_host_factory_.reset();
  }

  std::unique_ptr<MockRenderProcessHost> CreateRenderProcessHost() {
    return std::make_unique<MockRenderProcessHost>(browser_context_.get());
  }

  const std::map<int, scoped_refptr<SiteInstance>>& worker_process_map() {
    return process_manager_->worker_process_map_;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<ServiceWorkerProcessManager> process_manager_;
  GURL script_url_;
  std::unique_ptr<SiteInstanceRenderProcessHostFactory>
      render_process_host_factory_;
};

TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_WithProcessReuse) {
  const int kEmbeddedWorkerId = 100;
  const GURL kSiteUrl = GURL("http://example.com");
  SiteInfo site_info = SiteInfo::CreateForTesting(
      IsolationContext(browser_context_.get()), kSiteUrl);

  // Create a process that is hosting a frame with kSiteUrl.
  std::unique_ptr<MockRenderProcessHost> host(CreateRenderProcessHost());
  host->Init();
  RenderProcessHostImpl::AddFrameWithSite(browser_context_.get(), host.get(),
                                          site_info);

  const std::map<int, scoped_refptr<SiteInstance>>& processes =
      worker_process_map();
  EXPECT_TRUE(processes.empty());

  // Allocate a process to a worker, when process reuse is authorized.
  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  blink::ServiceWorkerStatusCode status =
      process_manager_->AllocateWorkerProcess(
          kEmbeddedWorkerId, script_url_,
          network::mojom::CrossOriginEmbedderPolicyValue::kNone,
          true /* can_use_existing_process */, AncestorFrameType::kNormalFrame,
          &process_info);

  // An existing process should be allocated to the worker.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_EQ(host->GetID(), process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::EXISTING_UNREADY_PROCESS,
            process_info.start_situation);
  EXPECT_EQ(1u, host->GetWorkerRefCount());
  EXPECT_EQ(1u, processes.size());
  auto found = processes.find(kEmbeddedWorkerId);
  ASSERT_TRUE(found != processes.end());
  EXPECT_EQ(host.get(), found->second->GetProcess());

  // Release the process.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  EXPECT_EQ(0u, host->GetWorkerRefCount());
  EXPECT_TRUE(processes.empty());

  RenderProcessHostImpl::RemoveFrameWithSite(browser_context_.get(), host.get(),
                                             site_info);
}

TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_WithoutProcessReuse) {
  const int kEmbeddedWorkerId = 100;
  const GURL kSiteUrl = GURL("http://example.com");
  SiteInfo site_info = SiteInfo::CreateForTesting(
      IsolationContext(browser_context_.get()), kSiteUrl);

  // Create a process that is hosting a frame with kSiteUrl.
  std::unique_ptr<MockRenderProcessHost> host(CreateRenderProcessHost());
  RenderProcessHostImpl::AddFrameWithSite(browser_context_.get(), host.get(),
                                          site_info);

  const std::map<int, scoped_refptr<SiteInstance>>& processes =
      worker_process_map();
  EXPECT_TRUE(processes.empty());

  // Allocate a process to a worker, when process reuse is disallowed.
  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  blink::ServiceWorkerStatusCode status =
      process_manager_->AllocateWorkerProcess(
          kEmbeddedWorkerId, script_url_,
          network::mojom::CrossOriginEmbedderPolicyValue::kNone,
          false /* can_use_existing_process */, AncestorFrameType::kNormalFrame,
          &process_info);

  // A new process should be allocated to the worker.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_NE(host->GetID(), process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::NEW_PROCESS,
            process_info.start_situation);
  EXPECT_EQ(0u, host->GetWorkerRefCount());
  EXPECT_EQ(1u, processes.size());
  auto found = processes.find(kEmbeddedWorkerId);
  ASSERT_TRUE(found != processes.end());
  EXPECT_NE(host.get(), found->second->GetProcess());

  // Release the process.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  EXPECT_TRUE(processes.empty());

  RenderProcessHostImpl::RemoveFrameWithSite(browser_context_.get(), host.get(),
                                             site_info);
}

TEST_F(ServiceWorkerProcessManagerTest, AllocateWorkerProcess_InShutdown) {
  process_manager_->Shutdown();
  ASSERT_TRUE(process_manager_->IsShutdown());

  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  blink::ServiceWorkerStatusCode status =
      process_manager_->AllocateWorkerProcess(
          1, script_url_, network::mojom::CrossOriginEmbedderPolicyValue::kNone,
          true /* can_use_existing_process */, AncestorFrameType::kNormalFrame,
          &process_info);

  // Allocating a process in shutdown should abort.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort, status);
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::UNKNOWN,
            process_info.start_situation);
  EXPECT_TRUE(worker_process_map().empty());
}

// Tests that ServiceWorkerProcessManager finds the appropriate process when
// inside a StoragePartition for guests (e.g., the <webview> tag).
// https://crbug.com/781313
TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_StoragePartitionForGuests) {
  // Allocate a process to a worker. It should use |script_url_| as the
  // site URL of the SiteInstance and a default StoragePartition.
  {
    const int kEmbeddedWorkerId = 55;  // dummy value
    ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
    blink::ServiceWorkerStatusCode status =
        process_manager_->AllocateWorkerProcess(
            kEmbeddedWorkerId, script_url_,
            network::mojom::CrossOriginEmbedderPolicyValue::kNone,
            true /* can_use_existing_process */,
            AncestorFrameType::kNormalFrame, &process_info);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
    EXPECT_EQ(
        GURL("http://example.com"),
        render_process_host_factory_->last_site_instance_used()->GetSiteURL());
    EXPECT_FALSE(
        render_process_host_factory_->last_site_instance_used()->IsGuest());
    auto* rph = RenderProcessHost::FromID(process_info.process_id);
    ASSERT_TRUE(rph);
    auto* storage_partition =
        static_cast<StoragePartitionImpl*>(rph->GetStoragePartition());
    EXPECT_TRUE(storage_partition->GetConfig().is_default());

    // Release the process.
    process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  }

  // Now change ServiceWorkerProcessManager to use a guest StoragePartition.
  // We must call |set_is_guest()| manually since the production codepath in
  // CreateRenderProcessHost() isn't hit here since we are using
  // RenderProcessHostFactory.
  const StoragePartitionConfig kGuestPartitionConfig =
      StoragePartitionConfig::Create(browser_context_.get(), "someapp",
                                     "somepartition", /*in_memory=*/false);
  scoped_refptr<SiteInstanceImpl> guest_site_instance =
      SiteInstanceImpl::CreateForGuest(browser_context_.get(),
                                       kGuestPartitionConfig);
  EXPECT_TRUE(guest_site_instance->IsGuest());
  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      browser_context_->GetStoragePartition(kGuestPartitionConfig));
  storage_partition->set_is_guest();
  process_manager_->set_storage_partition(storage_partition);

  // Allocate a process to a worker. It should be in the guest's
  // StoragePartition.
  {
    const int kEmbeddedWorkerId = 77;  // dummy value
    ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
    blink::ServiceWorkerStatusCode status =
        process_manager_->AllocateWorkerProcess(
            kEmbeddedWorkerId, script_url_,
            network::mojom::CrossOriginEmbedderPolicyValue::kNone,
            true /* can_use_existing_process */,
            AncestorFrameType::kNormalFrame, &process_info);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
    EXPECT_EQ(guest_site_instance->GetStoragePartitionConfig(),
              render_process_host_factory_->last_site_instance_used()
                  ->GetStoragePartitionConfig());
    EXPECT_TRUE(
        render_process_host_factory_->last_site_instance_used()->IsGuest());
    auto* rph = RenderProcessHost::FromID(process_info.process_id);
    ASSERT_TRUE(rph);
    EXPECT_EQ(rph->GetStoragePartition(), storage_partition);

    // Release the process.
    process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  }
}

class CustomSchemeContentClient : public TestContentClient {
 public:
  explicit CustomSchemeContentClient(std::string_view custom_scheme)
      : custom_scheme_(custom_scheme) {}

  void AddAdditionalSchemes(ContentClient::Schemes* schemes) override {
    schemes->standard_schemes.push_back(custom_scheme_);
    schemes->service_worker_schemes.push_back(custom_scheme_);
  }

 private:
  const std::string custom_scheme_;
};

class ScopedCustomSchemeContentBrowserClient : public ContentBrowserClient {
 public:
  explicit ScopedCustomSchemeContentBrowserClient(
      std::string_view custom_scheme)
      : custom_scheme_(custom_scheme) {
    old_client_ = SetBrowserClientForTesting(this);
  }

  ~ScopedCustomSchemeContentBrowserClient() override {
    SetBrowserClientForTesting(old_client_);
  }

  // `ContentBrowserClient`:
  bool IsHandledURL(const GURL& url) override { return true; }
  void GrantAdditionalRequestPrivilegesToWorkerProcess(
      int child_id,
      const GURL& script_url) override {
    if (script_url.SchemeIs(custom_scheme_)) {
      ChildProcessSecurityPolicy::GetInstance()->GrantRequestOrigin(
          child_id, url::Origin::Create(script_url));
    }
  }

 private:
  const std::string custom_scheme_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

class ServiceWorkerProcessManagerNonWebSchemeTest
    : public ServiceWorkerProcessManagerTest {
 public:
  ServiceWorkerProcessManagerNonWebSchemeTest() {
    ContentClient* old_content_client = GetContentClientForTesting();
    SetContentClient(&content_client_);
    ReRegisterContentSchemesForTests();
    SetContentClient(old_content_client);
  }

 private:
  url::ScopedSchemeRegistryForTests scheme_registry_;
  CustomSchemeContentClient content_client_{"non-web-scheme"};
  ScopedCustomSchemeContentBrowserClient browser_client_{"non-web-scheme"};
};

// Verifies that by default a Service Worker on a non-web scheme does not
// automatically have request permissions to its origin.
TEST_F(ServiceWorkerProcessManagerNonWebSchemeTest,
       NonWebSchemeWorkerCannotRequestOriginByDefault) {
  const int kEmbeddedWorkerId = 100;
  const GURL kUnknownNonWebSchemeUrl{"unknown-non-web-scheme://hostname"};

  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  blink::ServiceWorkerStatusCode status =
      process_manager_->AllocateWorkerProcess(
          kEmbeddedWorkerId, kUnknownNonWebSchemeUrl,
          network::mojom::CrossOriginEmbedderPolicyValue::kNone,
          /*can_use_existing_process=*/true, AncestorFrameType::kNormalFrame,
          &process_info);

  // A new process should be allocated to the worker.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::NEW_PROCESS,
            process_info.start_situation);

  // The process should not have access to its script's origin by default.
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
      process_info.process_id, kUnknownNonWebSchemeUrl));

  // Release the process.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
}

// Verifies that ContentBrowserClient can grant a new worker process access to
// origins.
TEST_F(ServiceWorkerProcessManagerNonWebSchemeTest,
       WorkerCanBeGrantedAccessToScriptOrigin) {
  if (!AreAllSitesIsolatedForTesting()) {
    GTEST_SKIP();
  }

  const int kEmbeddedWorkerId = 100;
  const GURL kNonWebSchemeUrl{"non-web-scheme://hostname"};

  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  blink::ServiceWorkerStatusCode status =
      process_manager_->AllocateWorkerProcess(
          kEmbeddedWorkerId, kNonWebSchemeUrl,
          network::mojom::CrossOriginEmbedderPolicyValue::kNone,
          /*can_use_existing_process=*/true, AncestorFrameType::kNormalFrame,
          &process_info);

  // A new process should be allocated to the worker.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::NEW_PROCESS,
            process_info.start_situation);

  // ScopedCustomSchemeContentBrowserClient should have granted the new
  // process access to the script's origin.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL(
      process_info.process_id, kNonWebSchemeUrl));

  // Release the process.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
}

}  // namespace content
