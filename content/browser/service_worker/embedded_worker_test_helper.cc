// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_test_helper.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/services/storage/service_worker/service_worker_storage_control_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace content {

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : EmbeddedWorkerTestHelper(
          user_data_directory,
          base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {}

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : browser_context_(std::make_unique<TestBrowserContext>()),
      render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      new_render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      quota_manager_(base::MakeRefCounted<storage::MockQuotaManager>(
          /*is_incognito=*/false,
          user_data_directory,
          base::ThreadTaskRunnerHandle::Get(),
          special_storage_policy)),
      quota_manager_proxy_(base::MakeRefCounted<storage::MockQuotaManagerProxy>(
          quota_manager_.get(),
          base::SequencedTaskRunnerHandle::Get())),
      wrapper_(base::MakeRefCounted<ServiceWorkerContextWrapper>(
          browser_context_.get())),
      fake_loader_factory_("HTTP/1.1 200 OK\nContent-Type: text/javascript\n\n",
                           "/* body */",
                           /*network_accessed=*/true,
                           net::OK),
      user_data_directory_(user_data_directory),
      database_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
      new_mock_render_process_id_(new_render_process_host_->GetID()),
      url_loader_factory_getter_(
          base::MakeRefCounted<URLLoaderFactoryGetter>()) {
  wrapper_->SetStorageControlBinderForTest(base::BindRepeating(
      &EmbeddedWorkerTestHelper::BindStorageControl, base::Unretained(this)));
  wrapper_->InitInternal(quota_manager_proxy_.get(),
                         special_storage_policy.get(),
                         /*blob_context=*/nullptr, browser_context_.get());
  wrapper_->process_manager()->SetProcessIdForTest(mock_render_process_id());
  wrapper_->process_manager()->SetNewProcessIdForTest(new_render_process_id());
  fake_loader_factory_wrapper_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &fake_loader_factory_);
  wrapper_->SetLoaderFactoryForUpdateCheckForTest(fake_loader_factory_wrapper_);

  render_process_host_->OverrideBinderForTesting(
      blink::mojom::EmbeddedWorkerInstanceClient::Name_,
      base::BindRepeating(&EmbeddedWorkerTestHelper::OnInstanceClientRequest,
                          base::Unretained(this)));
  new_render_process_host_->OverrideBinderForTesting(
      blink::mojom::EmbeddedWorkerInstanceClient::Name_,
      base::BindRepeating(&EmbeddedWorkerTestHelper::OnInstanceClientRequest,
                          base::Unretained(this)));
}

void EmbeddedWorkerTestHelper::AddPendingInstanceClient(
    std::unique_ptr<FakeEmbeddedWorkerInstanceClient> client) {
  pending_embedded_worker_instance_clients_.push(std::move(client));
}

void EmbeddedWorkerTestHelper::AddPendingServiceWorker(
    std::unique_ptr<FakeServiceWorker> service_worker) {
  pending_service_workers_.push(std::move(service_worker));
}

void EmbeddedWorkerTestHelper::OnInstanceClientReceiver(
    mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
        receiver) {
  std::unique_ptr<FakeEmbeddedWorkerInstanceClient> client;
  if (!pending_embedded_worker_instance_clients_.empty()) {
    // Use the instance client that was registered for this message.
    client = std::move(pending_embedded_worker_instance_clients_.front());
    pending_embedded_worker_instance_clients_.pop();
    if (!client) {
      // Some tests provide a nullptr to drop the request.
      return;
    }
  } else {
    client = CreateInstanceClient();
  }

  client->Bind(std::move(receiver));
  instance_clients_.insert(std::move(client));
}

void EmbeddedWorkerTestHelper::OnInstanceClientRequest(
    mojo::ScopedMessagePipeHandle request_handle) {
  mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient> receiver(
      std::move(request_handle));
  OnInstanceClientReceiver(std::move(receiver));
}

void EmbeddedWorkerTestHelper::OnServiceWorkerRequest(
    mojo::PendingReceiver<blink::mojom::ServiceWorker> receiver) {
  OnServiceWorkerReceiver(std::move(receiver));
}

void EmbeddedWorkerTestHelper::OnServiceWorkerReceiver(
    mojo::PendingReceiver<blink::mojom::ServiceWorker> receiver) {
  std::unique_ptr<FakeServiceWorker> service_worker;
  if (!pending_service_workers_.empty()) {
    // Use the service worker that was registered for this message.
    service_worker = std::move(pending_service_workers_.front());
    pending_service_workers_.pop();
    if (!service_worker) {
      // Some tests provide a nullptr to drop the request.
      return;
    }
  } else {
    service_worker = CreateServiceWorker();
  }

  service_worker->Bind(std::move(receiver));
  service_workers_.insert(std::move(service_worker));
}

void EmbeddedWorkerTestHelper::RemoveInstanceClient(
    FakeEmbeddedWorkerInstanceClient* instance_client) {
  auto it = instance_clients_.find(instance_client);
  instance_clients_.erase(it);
}

void EmbeddedWorkerTestHelper::RemoveServiceWorker(
    FakeServiceWorker* service_worker) {
  auto it = service_workers_.find(service_worker);
  service_workers_.erase(it);
}

EmbeddedWorkerTestHelper::~EmbeddedWorkerTestHelper() {
  // Call Detach() to invalidate the reference to `fake_loader_factory_` because
  // some tasks referring to the factory wrapper may use it after its
  // destruction.
  fake_loader_factory_wrapper_->Detach();
  if (wrapper_.get())
    wrapper_->Shutdown();
}

ServiceWorkerContextCore* EmbeddedWorkerTestHelper::context() {
  return wrapper_->context();
}

void EmbeddedWorkerTestHelper::ShutdownContext() {
  wrapper_->Shutdown();
  wrapper_ = nullptr;
}

void EmbeddedWorkerTestHelper::SimulateStorageRestartForTesting() {
  storage_control_.reset();
}

// static
std::unique_ptr<ServiceWorkerVersion::MainScriptResponse>
EmbeddedWorkerTestHelper::CreateMainScriptResponse() {
  network::mojom::URLResponseHead response_head;
  const char data[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: application/javascript\0"
      "\0";
  response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      std::string(data, std::size(data)));
  return std::make_unique<ServiceWorkerVersion::MainScriptResponse>(
      response_head);
}

void EmbeddedWorkerTestHelper::PopulateScriptCacheMap(
    int64_t version_id,
    base::OnceClosure callback) {
  scoped_refptr<ServiceWorkerVersion> version =
      context()->GetLiveVersion(version_id);
  if (!version) {
    std::move(callback).Run();
    return;
  }
  if (!version->GetMainScriptResponse())
    version->SetMainScriptResponse(CreateMainScriptResponse());
  if (!version->script_cache_map()->size()) {
    // Add a dummy ResourceRecord for the main script to the script cache map of
    // the ServiceWorkerVersion.
    WriteToDiskCacheAsync(
        context()->GetStorageControl(), version->script_url(), {} /* headers */,
        "I'm a body", "I'm a meta data",
        base::BindOnce(
            [](scoped_refptr<ServiceWorkerVersion> version,
               base::OnceClosure callback,
               storage::mojom::ServiceWorkerResourceRecordPtr record) {
              std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>
                  records;
              records.push_back(std::move(record));
              version->script_cache_map()->SetResources(records);

              std::move(callback).Run();
            },
            version, std::move(callback)));
    return;
  }
  // Call |callback| if |version| already has ResourceRecords.
  if (!callback.is_null())
    std::move(callback).Run();
}

std::unique_ptr<FakeEmbeddedWorkerInstanceClient>
EmbeddedWorkerTestHelper::CreateInstanceClient() {
  return std::make_unique<FakeEmbeddedWorkerInstanceClient>(this);
}

std::unique_ptr<FakeServiceWorker>
EmbeddedWorkerTestHelper::CreateServiceWorker() {
  return std::make_unique<FakeServiceWorker>(this);
}

void EmbeddedWorkerTestHelper::BindStorageControl(
    mojo::PendingReceiver<storage::mojom::ServiceWorkerStorageControl>
        receiver) {
  storage_control_ = std::make_unique<storage::ServiceWorkerStorageControlImpl>(
      user_data_directory_, database_task_runner_, std::move(receiver));
}

}  // namespace content
