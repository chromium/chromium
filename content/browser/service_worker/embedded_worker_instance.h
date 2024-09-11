// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/embedded_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/mojom/hid/hid.mojom-forward.h"
#endif

namespace content {

class CrossOriginEmbedderPolicyReporter;
class RenderProcessHost;
class ServiceWorkerContentSettingsProxyImpl;
class ServiceWorkerContextCore;
class ServiceWorkerVersion;
class StoragePartitionImpl;

namespace service_worker_new_script_loader_unittest {
class ServiceWorkerNewScriptLoaderTest;
FORWARD_DECLARE_TEST(ServiceWorkerNewScriptLoaderTest, AccessedNetwork);
}  // namespace service_worker_new_script_loader_unittest

// This gives an interface to control one EmbeddedWorker instance, which
// may be 'in-waiting' or running in one of the child processes added by
// AddProcessReference().
//
// Owned by ServiceWorkerVersion.
class CONTENT_EXPORT EmbeddedWorkerInstance
    : public blink::mojom::EmbeddedWorkerInstanceHost {
 public:
  class DevToolsProxy;
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)>;

  // This enum is used in UMA histograms. Append-only.
  enum StartingPhase {
    NOT_STARTING = 0,
    ALLOCATING_PROCESS = 1,
    // REGISTERING_TO_DEVTOOLS = 2,  // Obsolete
    SENT_START_WORKER = 3,
    SCRIPT_DOWNLOADING = 4,
    SCRIPT_LOADED = 5,
    // SCRIPT_EVALUATED = 6,  // Obsolete
    // THREAD_STARTED = 7,  // Obsolete
    // SCRIPT_READ_STARTED = 8,  // Obsolete
    // SCRIPT_READ_FINISHED = 9,  // Obsolete
    SCRIPT_STREAMING = 10,
    SCRIPT_EVALUATION = 11,
    // Add new values here and update enums.xml.
    STARTING_PHASE_MAX_VALUE,
  };

  // DEPRECATED, only for use by ServiceWorkerVersion.
  // TODO(crbug.com/41396417): Remove this interface.
  class Listener {
   public:
    virtual ~Listener() {}

    virtual void OnStarting() {}
    virtual void OnProcessAllocated() {}
    virtual void OnRegisteredToDevToolsManager() {}
    virtual void OnStartWorkerMessageSent() {}
    virtual void OnScriptLoaded() {}
    virtual void OnScriptEvaluationStart() {}
    virtual void OnStarted(
        blink::mojom::ServiceWorkerStartStatus status,
        blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type,
        bool has_hid_event_handlers,
        bool has_usb_event_handlers) {}

    // Called when status changed to STOPPING. The renderer has been sent a Stop
    // IPC message and OnStopped() will be called upon successful completion.
    virtual void OnStopping() {}

    // Called when status changed to STOPPED. Usually, this is called upon
    // receiving an ACK from renderer that the worker context terminated.
    // OnStopped() is also called if Stop() aborted an ongoing start attempt
    // even before the Start IPC message was sent to the renderer.  In this
    // case, OnStopping() is not called; the worker is "stopped" immediately
    // (the Start IPC is never sent).
    virtual void OnStopped(blink::EmbeddedWorkerStatus old_status) {}

    // Called when the browser-side IPC endpoint for communication with the
    // worker died. When this is called, status is STOPPED.
    virtual void OnDetached(blink::EmbeddedWorkerStatus old_status) {}

    virtual void OnReportException(const std::u16string& error_message,
                                   int line_number,
                                   int column_number,
                                   const GURL& source_url) {}
    virtual void OnReportConsoleMessage(
        blink::mojom::ConsoleMessageSource source,
        blink::mojom::ConsoleMessageLevel message_level,
        const std::u16string& message,
        int line_number,
        const GURL& source_url) {}
  };

  explicit EmbeddedWorkerInstance(ServiceWorkerVersion* owner_version);

  EmbeddedWorkerInstance(const EmbeddedWorkerInstance&) = delete;
  EmbeddedWorkerInstance& operator=(const EmbeddedWorkerInstance&) = delete;

  ~EmbeddedWorkerInstance() override;

  // Starts the worker. It is invalid to call this when the worker is not in
  // STOPPED status.
  //
  // |sent_start_callback| is invoked once the Start IPC is sent, and in some
  // cases may be invoked if an error prevented that from happening. It's not
  // invoked in some cases, like if the Mojo connection fails to connect, or
  // when Stop() is called and aborts the start procedure. Note that when the
  // callback is invoked with kOk status, the service worker has not yet
  // finished starting. Observe OnStarted()/OnStopped() for when start completed
  // or failed.
  void Start(blink::mojom::EmbeddedWorkerStartParamsPtr params,
             StatusCallback sent_start_callback);

  // Stops the worker. It is invalid to call this when the worker is not in
  // STARTING or RUNNING status.
  //
  // Stop() typically sends a Stop IPC to the renderer, and this instance enters
  // STOPPING status, with Listener::OnStopped() called upon completion. It can
  // synchronously complete if this instance is STARTING but the Start IPC
  // message has not yet been sent. In that case, the start procedure is
  // aborted, and this instance enters STOPPED status.
  //
  // May destroy `this`.
  void Stop();

  // Stops the worker if the worker is not being debugged (i.e. devtools is
  // not attached). This method is called by a stop-worker timer to kill
  // idle workers.
  //
  // May destroy `this`.
  void StopIfNotAttachedToDevTools();

  int embedded_worker_id() const { return embedded_worker_id_; }
  blink::EmbeddedWorkerStatus status() const { return status_; }
  StartingPhase starting_phase() const {
    DCHECK_EQ(blink::EmbeddedWorkerStatus::kStarting, status());
    return starting_phase_;
  }
  int restart_count() const { return restart_count_; }
  bool pause_initializing_global_scope() const {
    return pause_initializing_global_scope_;
  }
  void SetPauseInitializingGlobalScope();
  void ResumeInitializingGlobalScope();
  int process_id() const;
  int thread_id() const { return thread_id_; }
  int worker_devtools_agent_route_id() const;
  base::UnguessableToken WorkerDevtoolsId() const;

  // DEPRECATED, only for use by ServiceWorkerVersion.
  // TODO(crbug.com/41396417): Remove the Listener interface.
  void AddObserver(Listener* listener);
  void RemoveObserver(Listener* listener);

  void SetDevToolsAttached(bool attached);
  bool devtools_attached() const { return devtools_attached_; }

  bool network_accessed_for_script() const {
    return network_accessed_for_script_;
  }

  ServiceWorkerMetrics::StartSituation start_situation() const {
    DCHECK(status() == blink::EmbeddedWorkerStatus::kStarting ||
           status() == blink::EmbeddedWorkerStatus::kRunning);
    return start_situation_;
  }

  // Called when the main script load accessed the network.
  void OnNetworkAccessedForScriptLoad();

  // Called when the worker is installed.
  void OnWorkerVersionInstalled();

  // Called when the worker is doomed.
  void OnWorkerVersionDoomed();

  static std::string StatusToString(blink::EmbeddedWorkerStatus status);
  static std::string StartingPhaseToString(StartingPhase phase);

  // Forces this instance into STOPPED status and releases any state about the
  // running worker. Called when connection with the renderer died or the
  // renderer is unresponsive.  Essentially, it throws away any information
  // about the renderer-side worker, and frees this instance up to start a new
  // worker.
  // May destroy `this`.
  void Detach();

  // Examine the current state of the worker in order to determine if it should
  // require foreground priority or not.  This should be called whenever state
  // changes such that the decision might change.
  void UpdateForegroundPriority();

  // Pushes updated URL loader factories to the worker. Called during new worker
  // startup. Also called when DevTools network interception is enabled.
  // |subresource_bundle| is set to nullptr when only |script_bundle| is needed
  // to be updated.
  void UpdateLoaderFactories(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> script_bundle,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> subresource_bundle);

  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
      const storage::BucketLocator& bucket_locator);

#if !BUILDFLAG(IS_ANDROID)
  void BindHidService(const url::Origin& origin,
                      mojo::PendingReceiver<blink::mojom::HidService> receiver);
#endif  // !BUILDFLAG(IS_ANDROID)

  void BindUsbService(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);

  base::WeakPtr<EmbeddedWorkerInstance> AsWeakPtr();

  // The below can only be called on the UI thread. The returned factory may be
  // later supplied to UpdateLoaderFactories().
  //
  // `client_security_state` may be nullptr, in which case a default value is
  // set in the bundle.
  static std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
  CreateFactoryBundle(
      RenderProcessHost* rph,
      int routing_id,
      const blink::StorageKey& storage_key,
      network::mojom::ClientSecurityStatePtr client_security_state,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      ContentBrowserClient::URLLoaderFactoryType factory_type,
      const std::string& devtools_worker_token);

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
  GetCoepReporter();

 private:
  typedef base::ObserverList<Listener>::Unchecked ListenerList;
  struct StartInfo;
  class WorkerProcessHandle;
  friend class EmbeddedWorkerInstanceTestHarness;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, StartAndStop);
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, DetachDuringStart);
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest, StopDuringStart);
  FRIEND_TEST_ALL_PREFIXES(service_worker_new_script_loader_unittest::
                               ServiceWorkerNewScriptLoaderTest,
                           AccessedNetwork);

  void OnProcessAllocated(std::unique_ptr<WorkerProcessHandle> handle,
                          ServiceWorkerMetrics::StartSituation start_situation);
  void OnRegisteredToDevToolsManager(
      std::unique_ptr<DevToolsProxy> devtools_proxy);
  // Sends the StartWorker message to the renderer.
  void SendStartWorker(blink::mojom::EmbeddedWorkerStartParamsPtr params);

  // Implements blink::mojom::EmbeddedWorkerInstanceHost.
  void RequestTermination(RequestTerminationCallback callback) override;
  void CountFeature(blink::mojom::WebFeature feature) override;
  void OnReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent>,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>) override;
  void OnScriptLoaded() override;
  void OnScriptEvaluationStart() override;
  // Changes the internal worker status from STARTING to RUNNING.
  void OnStarted(
      blink::mojom::ServiceWorkerStartStatus status,
      blink::mojom::ServiceWorkerFetchHandlerType fetch_handler_type,
      bool has_hid_event_handlers,
      bool has_usb_event_handlers,
      int thread_id,
      blink::mojom::EmbeddedWorkerStartTimingPtr start_timing) override;
  // Resets the embedded worker instance to the initial state. Changes
  // the internal status from STARTING or RUNNING to STOPPED.
  // May destroy `this`.
  void OnStopped() override;
  void OnReportException(const std::u16string& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url) override;
  void OnReportConsoleMessage(blink::mojom::ConsoleMessageSource source,
                              blink::mojom::ConsoleMessageLevel message_level,
                              const std::u16string& message,
                              int line_number,
                              const GURL& source_url) override;

  // Resets all running state. After this function is called, |status_| is
  // kStopped.
  // May destroy `this`.
  void ReleaseProcess();

  // Called back from StartTask when the startup sequence failed. Calls
  // ReleaseProcess() and invokes |callback| with |status|. May destroy |this|.
  void OnSetupFailed(StatusCallback callback,
                     blink::ServiceWorkerStatusCode status);

  // Called when a foreground service worker is added/removed in a process.
  void NotifyForegroundServiceWorkerAdded();
  void NotifyForegroundServiceWorkerRemoved();

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  MakeScriptLoaderFactoryRemote(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle> script_bundle);

  void BindCacheStorageInternal();
  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
  GetCoepReporterInternal(StoragePartitionImpl* storage_partition);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  raw_ptr<ServiceWorkerVersion> owner_version_;

  // Unique within a ServiceWorkerContextCore.
  const int embedded_worker_id_;

  blink::EmbeddedWorkerStatus status_;
  StartingPhase starting_phase_;
  int restart_count_;

  // Pause initializing global scope when this flag is true
  // (https://crbug.com/1431792).
  bool pause_initializing_global_scope_ = false;

  // Current running information.
  std::unique_ptr<EmbeddedWorkerInstance::WorkerProcessHandle> process_handle_;
  int thread_id_;

  // |client_| is used to send messages to the renderer process. The browser
  // process should not disconnect the pipe because associated interfaces may be
  // using it. The renderer process will disconnect the pipe when appropriate.
  mojo::Remote<blink::mojom::EmbeddedWorkerInstanceClient> client_;

  mojo::AssociatedReceiver<EmbeddedWorkerInstanceHost> instance_host_receiver_{
      this};

  // Whether devtools is attached or not.
  bool devtools_attached_;

  // True if the script load request accessed the network. If the script was
  // served from HTTPCache or ServiceWorkerDatabase this value is false.
  bool network_accessed_for_script_;

  // True if the RenderProcessHost has been notified that this is a service
  // worker requiring foreground priority.
  bool foreground_notified_;

  ListenerList listener_list_;
  std::unique_ptr<DevToolsProxy> devtools_proxy_;

  // Contains info to be recorded on completing StartWorker sequence.
  // Set on Start() and cleared on OnStarted().
  std::unique_ptr<StartInfo> inflight_start_info_;

  // This is valid only after a process is allocated for the worker.
  ServiceWorkerMetrics::StartSituation start_situation_ =
      ServiceWorkerMetrics::StartSituation::UNKNOWN;

  std::unique_ptr<ServiceWorkerContentSettingsProxyImpl> content_settings_;

  mojo::SelfOwnedReceiverRef<network::mojom::URLLoaderFactory>
      script_loader_factory_;

  // Remote interface to talk to a running service worker. Used to update
  // subresource loader factories in the service worker.
  mojo::Remote<blink::mojom::SubresourceLoaderUpdater>
      subresource_loader_updater_;

  struct CacheStorageRequest {
    CacheStorageRequest(
        mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
        storage::BucketLocator bucket);
    CacheStorageRequest(CacheStorageRequest&& other);
    ~CacheStorageRequest();

    mojo::PendingReceiver<blink::mojom::CacheStorage> receiver;
    storage::BucketLocator bucket;
  };

  // Hold in-flight CacheStorage requests. They will be bound when the
  // ServiceWorker COEP header will be known.
  std::vector<CacheStorageRequest> pending_cache_storage_requests_;

  // COEP Reporter connected to the URLLoaderFactories that handles subresource
  // requests initiated from the service worker. The impl lives on the UI
  // thread, and |coep_reporter_| has the ownership of the impl instance.
  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter_;

  bool in_dtor_{false};

  base::WeakPtrFactory<EmbeddedWorkerInstance> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_INSTANCE_H_
