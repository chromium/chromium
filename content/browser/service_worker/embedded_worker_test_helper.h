// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/service_worker/fake_embedded_worker_instance_client.h"
#include "content/browser/service_worker/fake_service_worker.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"

namespace network {
class WeakWrapperSharedURLLoaderFactory;
}  // namespace network

namespace storage {
class ServiceWorkerStorageControlImpl;
}  // namespace storage

namespace content {

class FakeServiceWorker;
class MockRenderProcessHost;
class ReconnectableURLLoaderFactory;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;

// In-Process EmbeddedWorker test helper.
//
// Usage: create an instance of this class to test browser-side embedded worker
// code without creating a child process. This class will create a
// ServiceWorkerContextWrapper and ServiceWorkerContextCore for you.
//
// By default, this class uses FakeEmbeddedWorkerInstanceClient which notifies
// back success for StartWorker and StopWorker requests. It also uses
// FakeServiceWorker which returns success for event messages (e.g.
// InstallEvent, FetchEvent).
//
// Alternatively consumers can use subclasses of the Fake* classes
// to add their own logic/verification code.
//
// Example:
//
//  class MyClient : public FakeEmbeddedWorkerInstanceClient {
//    void StartWorker(...) override {
//      // Do custom stuff.
//      LOG(INFO) << "in start worker!";
//    }
//  };
//  class MyServiceWorker : public FakeServiceWorker {
//    void DispatchFetchEvent(...) override {
//      // Do custom stuff.
//      LOG(INFO) << "in fetch event!";
//    }
//  };
//
//  // Set up the fakes.
//  helper->AddPendingInstanceClient(std::make_unique<MyClient>());
//  helper->AddPendingServiceWorker(std::make_unique<MyServiceWorker>());
//
//  // Run code that starts a worker.
//  StartWorker();  // "in start worker!"
//
//  // Run code that dispatches a fetch event.
//  Navigate();  // "in fetch event!"
//
// See embedded_worker_instance_unittest.cc for more example usages.
class EmbeddedWorkerTestHelper {
 public:
  // If |user_data_directory| is empty, the context makes storage stuff in
  // memory.
  explicit EmbeddedWorkerTestHelper(const base::FilePath& user_data_directory);
  EmbeddedWorkerTestHelper(const base::FilePath& user_data_directory,
                           std::unique_ptr<BrowserContext> browser_context);
  EmbeddedWorkerTestHelper(
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy);
  EmbeddedWorkerTestHelper(
      const base::FilePath& user_data_directory,
      scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
      std::unique_ptr<BrowserContext> browser_context);

  EmbeddedWorkerTestHelper(const EmbeddedWorkerTestHelper&) = delete;
  EmbeddedWorkerTestHelper& operator=(const EmbeddedWorkerTestHelper&) = delete;

  virtual ~EmbeddedWorkerTestHelper();

  ServiceWorkerContextCore* context();
  ServiceWorkerContextWrapper* context_wrapper() { return wrapper_.get(); }
  void ShutdownContext();

  void SimulateStorageRestartForTesting();

  int GetNextThreadId() { return next_thread_id_++; }

  int mock_render_process_id() const { return mock_render_process_id_; }
  MockRenderProcessHost* mock_render_process_host() {
    return render_process_host_.get();
  }

  // Only used for tests that force creating a new render process.
  int new_render_process_id() const { return new_mock_render_process_id_; }

  storage::MockQuotaManager* quota_manager() { return quota_manager_.get(); }

  storage::MockQuotaManagerProxy* quota_manager_proxy() {
    return quota_manager_proxy_.get();
  }

  BrowserContext* browser_context() { return browser_context_.get(); }

  static std::unique_ptr<ServiceWorkerVersion::MainScriptResponse>
  CreateMainScriptResponse();

  scoped_refptr<network::SharedURLLoaderFactory> GetNetworkFactory();

  // Adds the given client to the pending queue. The next time this helper
  // receives a
  // mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient> (i.e., on
  // the next start worker attempt), it uses the first client from this queue.
  void AddPendingInstanceClient(
      std::unique_ptr<FakeEmbeddedWorkerInstanceClient> instance_client);

  // Adds the given service worker to the pending queue. The next time this
  // helper receives a mojo::PendingReceiver<blink::mojom::ServiceWorker>
  // receiver (i.e., on the next start worker attempt), it uses the first
  // service worker from this queue.
  void AddPendingServiceWorker(
      std::unique_ptr<FakeServiceWorker> service_worker);

  // A convenience method useful for keeping a pointer to a
  // FakeEmbeddedWorkerInstanceClient after it's added. Equivalent to:
  //   auto client_to_pass = std::make_unique<MockType>(args);
  //   auto* client = client.get();
  //   AddPendingInstanceClient(std::move(client_to_pass));
  template <typename MockType, typename... Args>
  MockType* AddNewPendingInstanceClient(Args&&... args);
  // Same for FakeServiceWorker.
  template <typename MockType, typename... Args>
  MockType* AddNewPendingServiceWorker(Args&&... args);

  /////////////////////////////////////////////////////////////////////////////
  // The following are exposed to public so the fake embedded worker and service
  // worker implementations and their subclasses can call them.

  // Called when |receiver| is received. It takes the object from a previous
  // AddPending*() call if any and calls Create*() otherwise.
  void OnInstanceClientReceiver(
      mojo::PendingReceiver<blink::mojom::EmbeddedWorkerInstanceClient>
          receiver);
  void OnServiceWorkerReceiver(
      mojo::PendingReceiver<blink::mojom::ServiceWorker> receiver);
  void OnInstanceClientRequest(mojo::ScopedMessagePipeHandle request_handle);
  void OnServiceWorkerRequest(
      mojo::PendingReceiver<blink::mojom::ServiceWorker> receiver);

  // Called by the fakes to destroy themselves.
  void RemoveInstanceClient(FakeEmbeddedWorkerInstanceClient* instance_client);
  void RemoveServiceWorker(FakeServiceWorker* service_worker);

  // Writes a dummy script into the given service worker's
  // ServiceWorkerScriptCacheMap. |callback| is called when done.
  virtual void PopulateScriptCacheMap(int64_t service_worker_version_id,
                                      base::OnceClosure callback);
  /////////////////////////////////////////////////////////////////////////////

  using RegistrationAndVersionPair =
      std::pair<scoped_refptr<ServiceWorkerRegistration>,
                scoped_refptr<ServiceWorkerVersion>>;

  RegistrationAndVersionPair PrepareRegistrationAndVersion(
      const GURL& scope,
      const GURL& script_url);

  // Calls worker->Start() and runs until the start IPC is sent.
  //
  // Expects success. For failure cases, call Start() manually.
  void StartWorkerUntilStartSent(
      EmbeddedWorkerInstance* worker,
      blink::mojom::EmbeddedWorkerStartParamsPtr params);

  // Calls worker->Start() and runs until startup finishes.
  //
  // Expects success. For failure cases, call Start() manually.
  void StartWorker(EmbeddedWorkerInstance* worker,
                   blink::mojom::EmbeddedWorkerStartParamsPtr params);

  blink::mojom::EmbeddedWorkerStartParamsPtr CreateStartParams(
      scoped_refptr<ServiceWorkerVersion> version);

  blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr CreateProviderInfo(
      scoped_refptr<ServiceWorkerVersion> version);

  mojo::PendingReceiver<blink::mojom::ServiceWorker> CreateServiceWorker(
      scoped_refptr<ServiceWorkerVersion> version);

  mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
  CreateController();

  blink::mojom::ServiceWorkerInstalledScriptsInfoPtr
  GetInstalledScriptsInfoPtr();

 protected:
  // Subclasses can override these to change the default fakes. This saves tests
  // from calling AddPending*() for each start worker attempt.
  virtual std::unique_ptr<FakeEmbeddedWorkerInstanceClient>
  CreateInstanceClient();
  virtual std::unique_ptr<FakeServiceWorker> CreateServiceWorker();

 private:
  void BindStorageControl(
      mojo::PendingReceiver<storage::mojom::ServiceWorkerStorageControl>
          receiver);

  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> render_process_host_;
  std::unique_ptr<MockRenderProcessHost> new_render_process_host_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

  FakeNetworkURLLoaderFactory fake_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      fake_loader_factory_wrapper_;

  const base::FilePath user_data_directory_;
  scoped_refptr<base::SequencedTaskRunner> database_task_runner_;
  std::unique_ptr<storage::ServiceWorkerStorageControlImpl> storage_control_;

  base::queue<std::unique_ptr<FakeEmbeddedWorkerInstanceClient>>
      pending_embedded_worker_instance_clients_;
  base::flat_set<std::unique_ptr<FakeEmbeddedWorkerInstanceClient>,
                 base::UniquePtrComparator>
      instance_clients_;

  base::queue<std::unique_ptr<FakeServiceWorker>> pending_service_workers_;
  base::flat_set<std::unique_ptr<FakeServiceWorker>, base::UniquePtrComparator>
      service_workers_;

  int next_thread_id_;
  int mock_render_process_id_;
  int new_mock_render_process_id_;

  scoped_refptr<ReconnectableURLLoaderFactory> url_loader_factory_;

  // Mojo endpoints.
  std::vector<mojo::Remote<blink::mojom::ControllerServiceWorker>> controllers_;
  std::vector<mojo::Remote<blink::mojom::ServiceWorkerInstalledScriptsManager>>
      installed_scripts_managers_;
  std::vector<mojo::PendingReceiver<
      blink::mojom::ServiceWorkerInstalledScriptsManagerHost>>
      installed_scripts_manager_host_receivers_;
};

template <typename MockType, typename... Args>
MockType* EmbeddedWorkerTestHelper::AddNewPendingInstanceClient(
    Args&&... args) {
  std::unique_ptr<MockType> mock =
      std::make_unique<MockType>(std::forward<Args>(args)...);
  MockType* mock_rawptr = mock.get();
  AddPendingInstanceClient(std::move(mock));
  return mock_rawptr;
}

template <typename MockType, typename... Args>
MockType* EmbeddedWorkerTestHelper::AddNewPendingServiceWorker(Args&&... args) {
  std::unique_ptr<MockType> mock =
      std::make_unique<MockType>(std::forward<Args>(args)...);
  MockType* mock_rawptr = mock.get();
  AddPendingServiceWorker(std::move(mock));
  return mock_rawptr;
}

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
