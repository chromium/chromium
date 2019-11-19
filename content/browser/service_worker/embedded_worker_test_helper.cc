// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_test_helper.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace content {

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : browser_context_(std::make_unique<TestBrowserContext>()),
      render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      new_render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      wrapper_(base::MakeRefCounted<ServiceWorkerContextWrapper>(
          browser_context_.get())),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
      new_mock_render_process_id_(new_render_process_host_->GetID()),
      url_loader_factory_getter_(
          base::MakeRefCounted<URLLoaderFactoryGetter>()) {
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      base::ThreadTaskRunnerHandle::Get();
  wrapper_->InitOnCoreThread(
      user_data_directory, std::move(database_task_runner), nullptr, nullptr,
      nullptr, url_loader_factory_getter_.get(),
      blink::ServiceWorkerUtils::IsImportedScriptUpdateCheckEnabled()
          ? wrapper_->CreateNonNetworkURLLoaderFactoryBundleInfoForUpdateCheck(
                browser_context_.get())
          : nullptr);
  wrapper_->process_manager()->SetProcessIdForTest(mock_render_process_id());
  wrapper_->process_manager()->SetNewProcessIdForTest(new_render_process_id());
  if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled())
    wrapper_->InitializeResourceContext(browser_context_->GetResourceContext());

  render_process_host_->OverrideBinderForTesting(
      blink::mojom::EmbeddedWorkerInstanceClient::Name_,
      base::BindRepeating(&EmbeddedWorkerTestHelper::OnInstanceClientRequest,
                          base::Unretained(this)));
  new_render_process_host_->OverrideBinderForTesting(
      blink::mojom::EmbeddedWorkerInstanceClient::Name_,
      base::BindRepeating(&EmbeddedWorkerTestHelper::OnInstanceClientRequest,
                          base::Unretained(this)));

  // Set a basic network URL loader factory so tests don't crash. Tests that
  // want to customize further should use URLLoaderInterceptor which will
  // override this.
  // TODO(falken): Just make all MockRenderProcessHosts create and own
  // their own url loader factory.
  default_network_loader_factory_ =
      std::make_unique<FakeNetworkURLLoaderFactory>();
  render_process_host_->OverrideURLLoaderFactory(
      default_network_loader_factory_.get());
  new_render_process_host_->OverrideURLLoaderFactory(
      default_network_loader_factory_.get());
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
    blink::mojom::ServiceWorkerRequest request) {
  mojo::PendingReceiver<blink::mojom::ServiceWorker> receiver(
      std::move(request));
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

// static
net::HttpResponseInfo EmbeddedWorkerTestHelper::CreateHttpResponseInfo() {
  net::HttpResponseInfo info;
  const char data[] =
      "HTTP/1.1 200 OK\0"
      "Content-Type: application/javascript\0"
      "\0";
  info.headers =
      new net::HttpResponseHeaders(std::string(data, base::size(data)));
  return info;
}

void EmbeddedWorkerTestHelper::PopulateScriptCacheMap(
    int64_t version_id,
    base::OnceClosure callback) {
  ServiceWorkerVersion* version = context()->GetLiveVersion(version_id);
  if (!version) {
    std::move(callback).Run();
    return;
  }
  if (!version->script_cache_map()->size()) {
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    // Add a dummy ResourceRecord for the main script to the script cache map of
    // the ServiceWorkerVersion.
    records.push_back(WriteToDiskCacheAsync(
        context()->storage(), version->script_url(),
        context()->storage()->NewResourceId(), {} /* headers */, "I'm a body",
        "I'm a meta data", std::move(callback)));
    version->script_cache_map()->SetResources(records);
  }
  if (!version->GetMainScriptHttpResponseInfo())
    version->SetMainScriptHttpResponseInfo(CreateHttpResponseInfo());
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

}  // namespace content
