// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include <string>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

// Keeps track of the most recent |site_instance| passed to
// CreateRenderProcessHost().
class SiteInstanceRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  SiteInstanceRenderProcessHostFactory() = default;
  ~SiteInstanceRenderProcessHostFactory() override = default;

  RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstance* site_instance) const override {
    processes_.push_back(
        std::make_unique<MockRenderProcessHost>(browser_context));

    // A spare RenderProcessHost is created with a null SiteInstance.
    if (site_instance)
      last_site_instance_used_ = site_instance;

    return processes_.back().get();
  }

  SiteInstance* last_site_instance_used() const {
    return last_site_instance_used_;
  }

 private:
  mutable std::vector<std::unique_ptr<MockRenderProcessHost>> processes_;
  mutable SiteInstance* last_site_instance_used_;

  DISALLOW_COPY_AND_ASSIGN(SiteInstanceRenderProcessHostFactory);
};

}  // namespace

class ServiceWorkerProcessManagerTest : public testing::Test {
 public:
  ServiceWorkerProcessManagerTest() {}

  void SetUp() override {
    browser_context_.reset(new TestBrowserContext);
    process_manager_.reset(
        new ServiceWorkerProcessManager(browser_context_.get()));
    script_url_ = GURL("http://www.example.com/sw.js");
    render_process_host_factory_.reset(
        new SiteInstanceRenderProcessHostFactory());
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
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<ServiceWorkerProcessManager> process_manager_;
  GURL script_url_;
  std::unique_ptr<SiteInstanceRenderProcessHostFactory>
      render_process_host_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProcessManagerTest);
};

TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_WithProcessReuse) {
  const int kEmbeddedWorkerId = 100;
  const GURL kSiteUrl = GURL("http://example.com");

  // Create a process that is hosting a frame with kSiteUrl.
  std::unique_ptr<MockRenderProcessHost> host(CreateRenderProcessHost());
  host->Init();
  RenderProcessHostImpl::AddFrameWithSite(browser_context_.get(), host.get(),
                                          kSiteUrl);

  const std::map<int, scoped_refptr<SiteInstance>>& processes =
      worker_process_map();
  EXPECT_TRUE(processes.empty());

  // Allocate a process to a worker, when process reuse is authorized.
  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  blink::ServiceWorkerStatusCode status =
      process_manager_->AllocateWorkerProcess(
          kEmbeddedWorkerId, script_url_, true /* can_use_existing_process */,
          &process_info);

  // An existing process should be allocated to the worker.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_EQ(host->GetID(), process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::EXISTING_UNREADY_PROCESS,
            process_info.start_situation);
  EXPECT_EQ(1u, host->GetKeepAliveRefCount());
  EXPECT_EQ(1u, processes.size());
  auto found = processes.find(kEmbeddedWorkerId);
  ASSERT_TRUE(found != processes.end());
  EXPECT_EQ(host.get(), found->second->GetProcess());

  // Release the process.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  EXPECT_EQ(0u, host->GetKeepAliveRefCount());
  EXPECT_TRUE(processes.empty());

  RenderProcessHostImpl::RemoveFrameWithSite(browser_context_.get(), host.get(),
                                             kSiteUrl);
}

TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_WithoutProcessReuse) {
  const int kEmbeddedWorkerId = 100;
  const GURL kSiteUrl = GURL("http://example.com");

  // Create a process that is hosting a frame with kSiteUrl.
  std::unique_ptr<MockRenderProcessHost> host(CreateRenderProcessHost());
  RenderProcessHostImpl::AddFrameWithSite(browser_context_.get(), host.get(),
                                          kSiteUrl);

  const std::map<int, scoped_refptr<SiteInstance>>& processes =
      worker_process_map();
  EXPECT_TRUE(processes.empty());

  // Allocate a process to a worker, when process reuse is disallowed.
  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  blink::ServiceWorkerStatusCode status =
      process_manager_->AllocateWorkerProcess(
          kEmbeddedWorkerId, script_url_, false /* can_use_existing_process */,
          &process_info);

  // A new process should be allocated to the worker.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  EXPECT_NE(host->GetID(), process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::NEW_PROCESS,
            process_info.start_situation);
  EXPECT_EQ(0u, host->GetKeepAliveRefCount());
  EXPECT_EQ(1u, processes.size());
  auto found = processes.find(kEmbeddedWorkerId);
  ASSERT_TRUE(found != processes.end());
  EXPECT_NE(host.get(), found->second->GetProcess());

  // Release the process.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  EXPECT_TRUE(processes.empty());

  RenderProcessHostImpl::RemoveFrameWithSite(browser_context_.get(), host.get(),
                                             kSiteUrl);
}

TEST_F(ServiceWorkerProcessManagerTest, AllocateWorkerProcess_InShutdown) {
  process_manager_->Shutdown();
  ASSERT_TRUE(process_manager_->IsShutdown());

  ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
  blink::ServiceWorkerStatusCode status =
      process_manager_->AllocateWorkerProcess(
          1, script_url_, true /* can_use_existing_process */, &process_info);

  // Allocating a process in shutdown should abort.
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kErrorAbort, status);
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, process_info.process_id);
  EXPECT_EQ(ServiceWorkerMetrics::StartSituation::UNKNOWN,
            process_info.start_situation);
  EXPECT_TRUE(worker_process_map().empty());
}

// Tests that ServiceWorkerProcessManager uses
// StoragePartitionImpl::site_for_service_worker() when it's set. This enables
// finding the appropriate process when inside a StoragePartition for guests
// (e.g., the <webview> tag). https://crbug.com/781313
TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_StoragePartitionForGuests) {
  const GURL kGuestSiteUrl =
      GURL(std::string(content::kGuestScheme).append("://someapp/somepath"));

  // Allocate a process to a worker. It should use |script_url_| as the
  // site URL of the SiteInstance.
  {
    const int kEmbeddedWorkerId = 55;  // dummy value
    ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
    blink::ServiceWorkerStatusCode status =
        process_manager_->AllocateWorkerProcess(
            kEmbeddedWorkerId, script_url_, true /* can_use_existing_process */,
            &process_info);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
    // Instead of testing the input to the CreateRenderProcessHost(), it'd be
    // more interesting to check the StoragePartition of the returned process
    // here and below. Alas, MockRenderProcessHosts always use the default
    // StoragePartition.
    EXPECT_EQ(
        GURL("http://example.com"),
        render_process_host_factory_->last_site_instance_used()->GetSiteURL());

    // Release the process.
    process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  }

  // Now change ServiceWorkerProcessManager to use a StoragePartition with
  // |site_for_service_worker| set. We must set |site_for_service_worker|
  // manually since the production codepath in CreateRenderProcessHost() isn't
  // hit here since we are using RenderProcessHostFactory.
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(browser_context_.get(), kGuestSiteUrl);
  // It'd be more realistic to create a non-default StoragePartition, but there
  // would be no added value to this test since MockRenderProcessHost is not
  // StoragePartition-aware.
  StoragePartitionImpl* storage_partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(browser_context_.get()));
  storage_partition->set_site_for_service_worker(site_instance->GetSiteURL());
  process_manager_->set_storage_partition(storage_partition);

  // Allocate a process to a worker. It should use kGuestSiteUrl instead of
  // |script_url_| as the site URL of the SiteInstance.
  {
    const int kEmbeddedWorkerId = 77;  // dummy value
    ServiceWorkerProcessManager::AllocatedProcessInfo process_info;
    blink::ServiceWorkerStatusCode status =
        process_manager_->AllocateWorkerProcess(
            kEmbeddedWorkerId, script_url_, true /* can_use_existing_process */,
            &process_info);
    EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
    EXPECT_EQ(
        kGuestSiteUrl,
        render_process_host_factory_->last_site_instance_used()->GetSiteURL());

    // Release the process.
    process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId);
  }
}

}  // namespace content
