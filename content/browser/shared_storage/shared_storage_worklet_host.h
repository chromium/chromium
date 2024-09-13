// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-forward.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class RenderProcessHost;
class SharedStorageDocumentServiceImpl;
class SharedStorageURLLoaderFactoryProxy;
class SharedStorageCodeCacheHostProxy;
class SharedStorageWorkletDriver;
class SharedStorageWorkletHostManager;
class StoragePartitionImpl;
class PageImpl;

// The SharedStorageWorkletHost is responsible for getting worklet operation
// requests (i.e. `addModule()`, `selectURL()`, `run()`) from the renderer (i.e.
// document that is hosting the worklet) and running it on the
// `SharedStorageWorkletService`. It will also handle the commands from the
// `SharedStorageWorkletService` (i.e. storage access, console log) which
// could happen while running those worklet operations.
//
// The SharedStorageWorkletHost lives in the `SharedStorageWorkletHostManager`,
// and the SharedStorageWorkletHost's lifetime is bounded by the earliest of the
// two timepoints:
// 1. When the outstanding worklet operations have finished on or after the
// document's destruction, but before the keepalive timeout.
// 2. The keepalive timeout is reached after the worklet's owner document is
// destroyed.
class CONTENT_EXPORT SharedStorageWorkletHost
    : public blink::mojom::SharedStorageWorkletHost,
      public blink::mojom::SharedStorageWorkletServiceClient {
 public:
  using BudgetResult = storage::SharedStorageManager::BudgetResult;

  using KeepAliveFinishedCallback =
      base::OnceCallback<void(SharedStorageWorkletHost*)>;

  enum class AddModuleState {
    kNotInitiated,
    kInitiated,
  };

  SharedStorageWorkletHost(
      SharedStorageDocumentServiceImpl& document_service,
      const url::Origin& frame_origin,
      const url::Origin& data_origin,
      const GURL& script_source_url,
      network::mojom::CredentialsMode credentials_mode,
      const std::vector<blink::mojom::OriginTrialFeature>&
          origin_trial_features,
      mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
          worklet_host,
      blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
          callback);
  ~SharedStorageWorkletHost() override;

  // blink::mojom::SharedStorageWorkletHost.
  void SelectURL(
      const std::string& name,
      std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
          urls_with_metadata,
      blink::CloneableMessage serialized_data,
      bool keep_alive_after_operation,
      blink::mojom::PrivateAggregationConfigPtr private_aggregation_config,
      SelectURLCallback callback) override;
  void Run(const std::string& name,
           blink::CloneableMessage serialized_data,
           bool keep_alive_after_operation,
           blink::mojom::PrivateAggregationConfigPtr private_aggregation_config,
           RunCallback callback) override;

  // Whether there are unfinished worklet operations (i.e. `addModule()`,
  // `selectURL()`, or `run()`.
  bool HasPendingOperations();

  // Called by the `SharedStorageWorkletHostManager` for this host to enter
  // keep-alive phase.
  void EnterKeepAliveOnDocumentDestroyed(KeepAliveFinishedCallback callback);

  // blink::mojom::SharedStorageWorkletServiceClient:
  void SharedStorageSet(const std::u16string& key,
                        const std::u16string& value,
                        bool ignore_if_present,
                        SharedStorageSetCallback callback) override;
  void SharedStorageAppend(const std::u16string& key,
                           const std::u16string& value,
                           SharedStorageAppendCallback callback) override;
  void SharedStorageDelete(const std::u16string& key,
                           SharedStorageDeleteCallback callback) override;
  void SharedStorageClear(SharedStorageClearCallback callback) override;
  void SharedStorageGet(const std::u16string& key,
                        SharedStorageGetCallback callback) override;
  void SharedStorageKeys(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener) override;
  void SharedStorageEntries(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener) override;
  void SharedStorageLength(SharedStorageLengthCallback callback) override;
  void SharedStorageRemainingBudget(
      SharedStorageRemainingBudgetCallback callback) override;
  void DidAddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                              const std::string& message) override;
  void RecordUseCounters(
      const std::vector<blink::mojom::WebFeature>& features) override;

  void ReportNoBinderForInterface(const std::string& error);

  // Returns the process host associated with the worklet. Returns nullptr if
  // the process has gone (e.g. during shutdown).
  RenderProcessHost* GetProcessHost() const;

  // Returns the creator frame of the worklet. Returns nullptr if the frame has
  // gone (e.g. during keep-alive phase).
  RenderFrameHostImpl* GetFrame();

  const GURL& script_source_url() const {
    return script_source_url_;
  }

 protected:
  // virtual for testing
  virtual void OnAddModuleOnWorkletFinished(
      blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
          callback,
      bool success,
      const std::string& error_message);

  virtual void OnRunOperationOnWorkletFinished(
      base::TimeTicks start_time,
      bool success,
      const std::string& error_message);

  virtual void OnRunURLSelectionOperationOnWorkletFinished(
      const GURL& urn_uuid,
      base::TimeTicks start_time,
      bool script_execution_succeeded,
      const std::string& script_execution_error_message,
      uint32_t index,
      BudgetResult budget_result);

  // Called if `keep_alive_after_operation_` is false, `IsInKeepAlivePhase()` is
  // false, and `pending_operations_count_` decrements back to 0u. Runs
  // `on_no_retention_operations_finished_callback_` to close the worklet.
  virtual void ExpireWorklet();

  // Returns whether the the worklet has entered keep-alive phase. During
  // keep-alive: the attempt to log console messages will be ignored; and the
  // completion of the last pending operation will terminate the worklet.
  bool IsInKeepAlivePhase() const;

  base::OneShotTimer& GetKeepAliveTimerForTesting() {
    return keep_alive_timer_;
  }

 private:
  class ScopedDevToolsHandle;

  void OnRunURLSelectionOperationOnWorkletScriptExecutionFinished(
      const GURL& urn_uuid,
      base::TimeTicks start_time,
      bool success,
      const std::string& error_message,
      uint32_t index);

  // Run `keep_alive_finished_callback_` to destroy `this`. Called when the last
  // pending operation has finished, or when a timeout is reached after entering
  // the keep-alive phase. `timeout_reached` indicates whether or not the
  // keep-alive is being terminated due to the timeout being reached.
  void FinishKeepAlive(bool timeout_reached);

  // Increment `pending_operations_count_`. Called when receiving an
  // `addModule()`, `selectURL()`, or `run()`.
  void IncrementPendingOperationsCount();

  // Decrement `pending_operations_count_`. Called when finishing handling an
  // `addModule()`, `selectURL()`, or `run()`.
  void DecrementPendingOperationsCount();

  // virtual for testing
  virtual base::TimeDelta GetKeepAliveTimeout() const;

  blink::mojom::SharedStorageWorkletService*
  GetAndConnectToSharedStorageWorkletService();

  // Constructs a `PrivateAggregationOperationDetails` object, including binding
  // a receiver to the `PrivateAggregationManager` and returning the
  // `PendingRemote`. If there is no `PrivateAggregationManger`, returns a null
  // pointer.
  blink::mojom::PrivateAggregationOperationDetailsPtr
  MaybeConstructPrivateAggregationOperationDetails(
      const blink::mojom::PrivateAggregationConfigPtr&
          private_aggregation_config);

  bool IsSharedStorageAllowed(
      std::string* out_debug_message,
      bool* out_block_is_site_setting_specific = nullptr);
  bool IsSharedStorageSelectURLAllowed(
      std::string* out_debug_message,
      bool* out_block_is_site_setting_specific);

  // RAII helper object for talking to `SharedStorageWorkletDevToolsManager`.
  std::unique_ptr<ScopedDevToolsHandle> devtools_handle_;

  // The URL of the module script. Set when `AddModuleOnWorklet` is invoked.
  GURL script_source_url_;

  // The origin trial features inherited from the creator document. Set when
  // `AddModuleOnWorklet` is invoked.
  std::vector<blink::mojom::OriginTrialFeature> origin_trial_features_;

  // Responsible for initializing the `SharedStorageWorkletService`.
  std::unique_ptr<SharedStorageWorkletDriver> driver_;

  // When this worklet is created, `document_service_` is set to the associated
  // `SharedStorageDocumentServiceImpl` and is always valid. Reset automatically
  // when `SharedStorageDocumentServiceImpl` is destroyed. Note that this
  // `SharedStorageWorkletHost` may outlive the `page_` due to its keep-alive.
  base::WeakPtr<SharedStorageDocumentServiceImpl> document_service_;

  // When this worklet is created, `page_` is set to the associated `PageImpl`
  // and is always valid. Reset automatically when `PageImpl` is destroyed.
  // Note that this `SharedStorageWorkletHost` may outlive the `page_` due to
  // its keep-alive.
  base::WeakPtr<PageImpl> page_;

  // Storage partition. Used to get other raw pointers below, as well as a
  // URLLoaderFactory for the browser process, which is used for reporting.
  // Don't store a reference to that URLLoaderFactory directly to avoid
  // confusion with `url_loader_factory_proxy_`.
  raw_ptr<StoragePartitionImpl> storage_partition_;

  // Both `this` and `shared_storage_manager_` live in the `StoragePartition`.
  // `shared_storage_manager_` always outlives `this` because `this` will be
  // destroyed before `shared_storage_manager_` in ~StoragePartition.
  raw_ptr<storage::SharedStorageManager> shared_storage_manager_;

  // The owning `SharedStorageWorkletHostManager`, which will outlive `this`.
  raw_ptr<SharedStorageWorkletHostManager> shared_storage_worklet_host_manager_;

  // Pointer to the `BrowserContext`, saved to be able to call
  // `IsSharedStorageAllowed()`, and to get the global URLLoaderFactory.
  raw_ptr<BrowserContext> browser_context_;

  // The shared storage worklet's origin and site for data access and permission
  // checks.
  url::Origin shared_storage_origin_;
  net::SchemefulSite shared_storage_site_;

  // To avoid race conditions associated with top frame navigations and to be
  // able to call `IsSharedStorageAllowed()` during keep-alive, we need to save
  // the value of the main frame origin in the constructor.
  const url::Origin main_frame_origin_;

  // Whether `shared_storage_origin_` is same origin with the creator context's
  // origin.
  bool is_same_origin_worklet_;

  // A map of unresolved URNs to the candidate URL with metadata vector. Inside
  // `RunURLSelectionOperationOnWorklet()` a new URN is generated and is
  // inserted into `unresolved_urns_`. When the corresponding
  // `OnRunURLSelectionOperationOnWorkletFinished()` is called, the URN is
  // removed from `unresolved_urns_`.
  std::map<GURL, std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>>
      unresolved_urns_;

  // The number of unfinished worklet requests, including `addModule()`,
  // `selectURL()`, or `run()`.
  uint32_t pending_operations_count_ = 0u;

  // Whether or not the lifetime of the worklet should be extended beyond when
  // the `pending_operations_count_` returns to 0. If false, the worklet will
  // be closed as soon as the count next reaches 0 after being positive. This
  // bool is updated with each call to `run()` or `selectURL()`.
  bool keep_alive_after_operation_ = true;

  // Timer for starting and ending the keep-alive phase.
  base::OneShotTimer keep_alive_timer_;

  // Time when worklet host is constructed.
  base::TimeTicks creation_time_;

  // Last time when `pending_operations_count_` reaches 0u after being positive.
  base::TimeTicks last_operation_finished_time_;

  // Time when worklet host entered keep-alive, if applicable.
  base::TimeTicks enter_keep_alive_time_;

  // Tracks whether the worklet has ever been kept-alive (in order to be
  // recorded in a histogram via the destructor), and if so, what caused the
  // keep-alive to be terminated.
  blink::SharedStorageWorkletDestroyedStatus destroyed_status_ =
      blink::SharedStorageWorkletDestroyedStatus::kDidNotEnterKeepAlive;

  // Set when the worklet host enters keep-alive phase.
  KeepAliveFinishedCallback keep_alive_finished_callback_;

  // Receives selectURL() and run() operations.
  mojo::AssociatedReceiver<blink::mojom::SharedStorageWorkletHost> receiver_{
      this};

  // Both `shared_storage_worklet_service_`
  // and `shared_storage_worklet_service_client_` are bound in
  // GetAndConnectToSharedStorageWorkletService().
  //
  // `shared_storage_worklet_service_client_` is associated specifically with
  // the pipe that `shared_storage_worklet_service_` runs on, as we want the
  // messages initiated from the worklet (e.g. storage access, console log) to
  // be well ordered with respect to the corresponding request's callback
  // message which will be interpreted as the completion of an operation.
  mojo::Remote<blink::mojom::SharedStorageWorkletService>
      shared_storage_worklet_service_;

  mojo::AssociatedReceiver<blink::mojom::SharedStorageWorkletServiceClient>
      shared_storage_worklet_service_client_{this};

  // The proxy is used to limit the request that the worklet can make, e.g. to
  // ensure the URL is not modified by a compromised worklet; to enforce the
  // application/javascript request header; to enforce same-origin mode; etc.
  std::unique_ptr<SharedStorageURLLoaderFactoryProxy> url_loader_factory_proxy_;

  // The proxy is used to limit the request that the worklet can make, e.g. to
  // ensure the URL is not modified by a compromised worklet. This is reset
  // after the script loading finishes, to prevent leaking the shared storage
  // data after that.
  std::unique_ptr<SharedStorageCodeCacheHostProxy> code_cache_host_proxy_;

  // Handles code cache requests after being proxied from
  // `SharedStorageCodeCacheHostProxy`.
  std::unique_ptr<CodeCacheHostImpl::ReceiverSet> code_cache_host_receivers_;

  // BrowserInterfaceBroker implementation through which this
  // SharedStorageWorkletHost exposes Mojo services to the corresponding worklet
  // in the renderer.
  //
  // The interfaces that can be requested from this broker are defined in the
  // content/browser/browser_interface_binders.cc file, in the functions which
  // take a `SharedStorageWorkletHost*` parameter.
  BrowserInterfaceBrokerImpl<SharedStorageWorkletHost,
                             SharedStorageWorkletHost*>
      broker_{this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  base::WeakPtrFactory<SharedStorageWorkletHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_
