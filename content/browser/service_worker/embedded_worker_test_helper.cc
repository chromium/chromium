// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_test_helper.h"

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "components/services/storage/service_worker/service_worker_storage_control_impl.h"
#include "content/browser/loader/reconnectable_url_loader_factory.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/policy_container_utils.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace content {

namespace {

void CreateURLLoaderFactory(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>* out_factory) {
  network::URLLoaderFactoryBuilder factory_builder;
  if (url_loader_factory::GetTestingInterceptor()) {
    url_loader_factory::GetTestingInterceptor().Run(
        network::mojom::kBrowserProcessId, factory_builder);
  }

  // Requests are expected to be intercepted by
  // `url_loader_factory::GetTestingInterceptor()` and not to reach
  // `terminal_factory` here. So `terminal_factory` is not bound to an actual
  // receiver.
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> terminal_factory;

  *out_factory =
      std::move(factory_builder)
          .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
              terminal_factory.InitWithNewPipeAndPassRemote());
}

}  // namespace

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : EmbeddedWorkerTestHelper(
          user_data_directory,
          base::MakeRefCounted<storage::MockSpecialStoragePolicy>(),
          std::make_unique<TestBrowserContext>()) {}

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : EmbeddedWorkerTestHelper(user_data_directory,
                               special_storage_policy,
                               std::make_unique<TestBrowserContext>()) {}

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory,
    std::unique_ptr<BrowserContext> browser_context)
    : EmbeddedWorkerTestHelper(
          user_data_directory,
          base::MakeRefCounted<storage::MockSpecialStoragePolicy>(),
          std::move(browser_context)) {}

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<BrowserContext> browser_context)
    : browser_context_(std::move(browser_context)),
      render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      new_render_process_host_(
          std::make_unique<MockRenderProcessHost>(browser_context_.get())),
      quota_manager_(base::MakeRefCounted<storage::MockQuotaManager>(
          /*is_incognito=*/false,
          user_data_directory,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          special_storage_policy)),
      quota_manager_proxy_(base::MakeRefCounted<storage::MockQuotaManagerProxy>(
          quota_manager_.get(),
          base::SequencedTaskRunner::GetCurrentDefault())),
      wrapper_(base::MakeRefCounted<ServiceWorkerContextWrapper>(
          browser_context_.get())),
      fake_loader_factory_("HTTP/1.1 200 OK\nContent-Type: text/javascript\n\n",
                           "/* body */",
                           /*network_accessed=*/true,
                           net::OK),
      user_data_directory_(user_data_directory),
      database_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
      new_mock_render_process_id_(new_render_process_host_->GetID()),
      url_loader_factory_(base::MakeRefCounted<ReconnectableURLLoaderFactory>(
          base::BindRepeating(&CreateURLLoaderFactory))) {
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

scoped_refptr<network::SharedURLLoaderFactory>
EmbeddedWorkerTestHelper::GetNetworkFactory() {
  return network::SharedURLLoaderFactory::Create(url_loader_factory_->Clone());
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

EmbeddedWorkerTestHelper::RegistrationAndVersionPair
EmbeddedWorkerTestHelper::PrepareRegistrationAndVersion(
    const GURL& scope,
    const GURL& script_url) {
  RegistrationAndVersionPair pair;
  blink::mojom::ServiceWorkerRegistrationOptions options;
  options.scope = scope;
  pair.first = CreateNewServiceWorkerRegistration(
      context()->registry(), options,
      blink::StorageKey::CreateFirstParty(url::Origin::Create(scope)));
  pair.second = CreateNewServiceWorkerVersion(
      context()->registry(), pair.first, script_url,
      blink::mojom::ScriptType::kClassic);
  return pair;
}

// Calls worker->Start() and runs until the start IPC is sent.
//
// Expects success. For failure cases, call Start() manually.
void EmbeddedWorkerTestHelper::StartWorkerUntilStartSent(
    EmbeddedWorkerInstance* worker,
    blink::mojom::EmbeddedWorkerStartParamsPtr params) {
  base::test::TestFuture<blink::ServiceWorkerStatusCode> future;
  worker->Start(std::move(params), future.GetCallback());
  EXPECT_EQ(blink::ServiceWorkerStatusCode::kOk, future.Get());
}

// Calls worker->Start() and runs until startup finishes.
//
// Expects success. For failure cases, call Start() manually.
void EmbeddedWorkerTestHelper::StartWorker(
    EmbeddedWorkerInstance* worker,
    blink::mojom::EmbeddedWorkerStartParamsPtr params) {
  StartWorkerUntilStartSent(worker, std::move(params));
  // TODO(falken): Listen for OnStarted() instead of this.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(blink::EmbeddedWorkerStatus::kRunning, worker->status());
}

blink::mojom::EmbeddedWorkerStartParamsPtr
EmbeddedWorkerTestHelper::CreateStartParams(
    scoped_refptr<ServiceWorkerVersion> version) {
  auto params = blink::mojom::EmbeddedWorkerStartParams::New();
  params->service_worker_version_id = version->version_id();
  params->scope = version->scope();
  params->script_url = version->script_url();
  params->is_installed = false;

  params->service_worker_receiver = CreateServiceWorker(version);
  params->controller_receiver = CreateController();
  params->installed_scripts_info = GetInstalledScriptsInfoPtr();
  params->provider_info = CreateProviderInfo(std::move(version));
  params->policy_container = CreateStubPolicyContainer();
  // Set a fake cors_exempt_header_list here instead of taking from the browser
  // because the current ServiceWorkerContextWrapper doesn't have
  // storage_partition. It's possible to set the storage partition but prefer
  // this simple list for testing.
  params->cors_exempt_header_list = std::vector<std::string>{"X-Exempt-Test"};
  return params;
}

blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr
EmbeddedWorkerTestHelper::CreateProviderInfo(
    scoped_refptr<ServiceWorkerVersion> version) {
  CHECK(version);
  auto provider_info =
      blink::mojom::ServiceWorkerProviderInfoForStartWorker::New();
  version->worker_host_ = std::make_unique<ServiceWorkerHost>(
      provider_info->host_remote.InitWithNewEndpointAndPassReceiver(), *version,
      context()->AsWeakPtr());
  return provider_info;
}

mojo::PendingReceiver<blink::mojom::ServiceWorker>
EmbeddedWorkerTestHelper::CreateServiceWorker(
    scoped_refptr<ServiceWorkerVersion> version) {
  version->service_worker_remote_.reset();
  return version->service_worker_remote_.BindNewPipeAndPassReceiver();
}

mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
EmbeddedWorkerTestHelper::CreateController() {
  controllers_.emplace_back();
  return controllers_.back().BindNewPipeAndPassReceiver();
}

blink::mojom::ServiceWorkerInstalledScriptsInfoPtr
EmbeddedWorkerTestHelper::GetInstalledScriptsInfoPtr() {
  installed_scripts_managers_.emplace_back();
  auto info = blink::mojom::ServiceWorkerInstalledScriptsInfo::New();
  info->manager_receiver =
      installed_scripts_managers_.back().BindNewPipeAndPassReceiver();
  installed_scripts_manager_host_receivers_.push_back(
      info->manager_host_remote.InitWithNewPipeAndPassReceiver());
  return info;
}

}  // namespace content
