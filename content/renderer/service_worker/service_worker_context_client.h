// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/containers/id_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom-forward.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/public/web/web_embedded_worker.h"
#include "v8/include/v8-forward.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace blink {
class ChildURLLoaderFactoryBundle;
class WebServiceWorkerContextProxy;
class WebURLResponse;
struct WebServiceWorkerInstalledScriptsManagerParams;
}

namespace content {

class BlinkInterfaceRegistryImpl;
class EmbeddedWorkerInstanceClientImpl;

// ServiceWorkerContextClient is a "client" of a service worker execution
// context. It enables communication between the embedder and Blink's
// ServiceWorkerGlobalScope. It is created when the service worker begins
// starting up, and destroyed when the service worker stops. It is owned by
// WebEmbeddedWorkerImpl (which is owned by EmbeddedWorkerInstanceClientImpl).
//
// This class is created and destroyed on the "initiator" thread. The initiator
// thread is the thread that constructs this class. Currently it's the main
// thread but could be the IO thread in the future. https://crbug.com/692909
//
// Unless otherwise noted (here or in base class documentation), all methods
// are called on the worker thread.
class ServiceWorkerContextClient
    : public blink::WebServiceWorkerContextClient,
      public service_manager::mojom::InterfaceProvider {
 public:
  // Called on the initiator thread.
  // - |is_starting_installed_worker| is true if the script is already installed
  //   and will be streamed from the browser process.
  // - |owner| must outlive this new instance.
  // - |start_timing| should be initially populated with
  //   |start_worker_received_time|. This instance will fill in the rest during
  //   startup.
  // - |subresource_loader_updater| is a mojo receiver that will be bound to
  //   ServiceWorkerFetchContextImpl. This interface is used to update
  //   subresource loader factories.
  // - |script_url_to_skip_throttling| is the URL of the service worker script
  //   that already started being loaded by the browser process due to the
  //   update check, or the empty URL if there is no such script. See also
  //   comments in EmbeddedWorkerStartParams::script_url_to_skip_throttling.
  ServiceWorkerContextClient(
      int64_t service_worker_version_id,
      const GURL& service_worker_scope,
      const GURL& script_url,
      bool is_starting_installed_worker,
      const blink::RendererPreferences& renderer_preferences,
      mojo::PendingReceiver<blink::mojom::ServiceWorker>
          service_worker_receiver,
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
          controller_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>
          instance_host,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
          interface_provider,
      blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
      EmbeddedWorkerInstanceClientImpl* owner,
      blink::mojom::EmbeddedWorkerStartTimingPtr start_timing,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          preference_watcher_receiver,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_loaders,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          subresource_loader_updater,
      const GURL& script_url_to_skip_throttling,
      scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner,
      int32_t service_worker_route_id,
      const std::vector<std::string>& cors_exempt_header_list,
      const blink::StorageKey& storage_key,
      const blink::ServiceWorkerToken& service_worker_token);

  ServiceWorkerContextClient(const ServiceWorkerContextClient&) = delete;
  ServiceWorkerContextClient& operator=(const ServiceWorkerContextClient&) =
      delete;

  // Called on the initiator thread.
  ~ServiceWorkerContextClient() override;

  // Called on the initiator thread.
  void StartWorkerContextOnInitiatorThread(
      std::unique_ptr<blink::WebEmbeddedWorker> worker,
      std::unique_ptr<blink::WebEmbeddedWorkerStartData> start_data,
      std::unique_ptr<blink::WebServiceWorkerInstalledScriptsManagerParams>,
      mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
          content_settings,
      mojo::PendingRemote<blink::mojom::CacheStorage> cache_storage,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker);

  // Called on the initiator thread.
  blink::WebEmbeddedWorker& worker();

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // WebServiceWorkerContextClient overrides.
  void WorkerReadyForInspectionOnInitiatorThread(
      blink::CrossVariantMojoRemote<blink::mojom::DevToolsAgentInterfaceBase>
          devtools_agent_remote,
      blink::CrossVariantMojoReceiver<
          blink::mojom::DevToolsAgentHostInterfaceBase>
          devtools_agent_host_receiver) override;
  void FailedToFetchClassicScript() override;
  void FailedToFetchModuleScript() override;
  void WorkerScriptLoadedOnWorkerThread() override;
  void WorkerContextStarted(
      blink::WebServiceWorkerContextProxy* proxy,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner) override;
  void WillEvaluateScript(v8::Local<v8::Context> v8_context) override;
  void DidEvaluateScript(bool success) override;
  void WillInitializeWorkerContext() override;
  void WillDestroyWorkerContext(v8::Local<v8::Context> context) override;
  void WorkerContextDestroyed() override;
  void CountFeature(blink::mojom::WebFeature feature) override;
  void ReportException(const blink::WebString& error_message,
                       int line_number,
                       int column_number,
                       const blink::WebString& source_url) override;
  void ReportConsoleMessage(blink::mojom::ConsoleMessageSource source,
                            blink::mojom::ConsoleMessageLevel level,
                            const blink::WebString& message,
                            int line_number,
                            const blink::WebString& source_url) override;
  void SetupNavigationPreload(int fetch_event_id,
                              const blink::WebURL& url,
                              blink::CrossVariantMojoReceiver<
                                  network::mojom::URLLoaderClientInterfaceBase>
                                  preload_url_loader_client_receiver) override;
  void RequestTermination(RequestTerminationCallback callback) override;
  bool ShouldNotifyServiceWorkerOnWebSocketActivity(
      v8::Local<v8::Context> context) override;
  scoped_refptr<blink::WebServiceWorkerFetchContext>
  CreateWorkerFetchContextOnInitiatorThread() override;
  void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<blink::WebURLResponse> response,
      mojo::ScopedDataPipeConsumerHandle data_pipe) override;
  void OnNavigationPreloadComplete(int fetch_event_id,
                                   base::TimeTicks completion_time,
                                   int64_t encoded_data_length,
                                   int64_t encoded_body_length,
                                   int64_t decoded_body_length) override;
  void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<blink::WebServiceWorkerError> error) override;

 private:
  struct WorkerContextData;
  friend class ControllerServiceWorkerImpl;
  friend class ServiceWorkerContextClientTest;
  FRIEND_TEST_ALL_PREFIXES(
      ServiceWorkerContextClientTest,
      DispatchOrQueueFetchEvent_RequestedTerminationAndDie);
  FRIEND_TEST_ALL_PREFIXES(
      ServiceWorkerContextClientTest,
      DispatchOrQueueFetchEvent_RequestedTerminationAndWakeUp);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextClientTest,
                           DispatchOrQueueFetchEvent_NotRequestedTermination);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerContextClientTest, TaskInServiceWorker);

  void SendWorkerStarted(blink::mojom::ServiceWorkerStartStatus status);

  // Stops the worker context. Called on the initiator thread.
  void StopWorkerOnInitiatorThread();

  base::WeakPtr<ServiceWorkerContextClient> GetWeakPtr();

  const int64_t service_worker_version_id_;
  const GURL service_worker_scope_;
  const GURL script_url_;
  // True if this service worker was already installed at worker
  // startup time.
  const bool is_starting_installed_worker_;

  // See comments in EmbeddedWorkerStartParams::script_url_to_skip_throttling.
  const GURL script_url_to_skip_throttling_;

  blink::RendererPreferences renderer_preferences_;
  // Passed on creation of ServiceWorkerFetchContext.
  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_receiver_;

  scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  // Not owned; |this| is destroyed when |proxy_| becomes invalid.
  raw_ptr<blink::WebServiceWorkerContextProxy> proxy_;

  // These Mojo objects are bound on the worker thread.
  mojo::PendingReceiver<blink::mojom::ServiceWorker>
      pending_service_worker_receiver_;
  mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
      controller_receiver_;
  mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
      pending_interface_provider_receiver_;
  mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
      pending_subresource_loader_updater_;

  // Holds renderer interfaces exposed to the browser.
  service_manager::BinderRegistry registry_;
  std::unique_ptr<BlinkInterfaceRegistryImpl> blink_interface_registry_;

  // Receiver for the InterfaceProvider interface which is used by the browser
  // to request interfaces that are exposed by the renderer. Bound and destroyed
  // on the worker task runner.
  mojo::Receiver<service_manager::mojom::InterfaceProvider>
      interface_provider_receiver_{this};

  // This is bound on the initiator thread.
  mojo::SharedAssociatedRemote<blink::mojom::EmbeddedWorkerInstanceHost>
      instance_host_;

  // This holds blink.mojom.ServiceWorkerContainer(Host) connections to the
  // browser-side ServiceWorkerHost to keep it alive there.
  // Note: |service_worker_provider_info_->script_loader_factory_remote| is
  // moved to WebServiceWorkerNetworkProviderImpl when
  // CreateServiceWorkerNetworkProvider is called.
  blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr
      service_worker_provider_info_;

  // Must be accessed on the initiator thread only.
  raw_ptr<EmbeddedWorkerInstanceClientImpl> owner_;

  // Initialized on the worker thread in WorkerContextStarted and
  // destructed on the worker thread in WillDestroyWorkerContext.
  //
  // WARNING: This can be cleared at nearly any time, since WillDestroyContext
  // is called by Blink when it decides to terminate the worker thread. This
  // includes during event dispatch if a JavaScript debugger breakpoint pauses
  // execution (see issue 934622). It should be safe to assume |context_| is
  // valid at the start of a task that was posted to |worker_task_runner_|, as
  // that is from WorkerThread::GetTaskRunner() which safely drops the task on
  // worker termination.
  std::unique_ptr<WorkerContextData> context_;

  // Accessed on the worker thread. Passed to the browser process after worker
  // startup completes.
  blink::mojom::EmbeddedWorkerStartTimingPtr start_timing_;

  // A URLLoaderFactory instance used for subresource loading.
  scoped_refptr<blink::ChildURLLoaderFactoryBundle> loader_factories_;

  // Out-of-process NetworkService:
  // Detects disconnection from the network service.
  mojo::Remote<network::mojom::URLLoaderFactory>
      network_service_disconnect_handler_holder_;

  std::unique_ptr<blink::WebEmbeddedWorker> worker_;

  int32_t service_worker_route_id_;

  std::vector<std::string> cors_exempt_header_list_;

  base::TimeTicks top_level_script_loading_start_time_ = base::TimeTicks::Now();

  blink::StorageKey storage_key_;

  blink::ServiceWorkerToken service_worker_token_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_CLIENT_H_
