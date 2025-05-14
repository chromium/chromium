// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/schemeful_site.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/shared_storage.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom.h"
#include "third_party/blink/public/mojom/origin_trials/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-forward.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace content {

class BrowserContext;
struct GlobalRenderFrameHostId;
class RenderFrameHostImpl;
class RenderProcessHost;
class SharedStorageDocumentServiceImpl;
class SharedStorageURLLoaderFactoryProxy;
class SharedStorageCodeCacheHostProxy;
class SharedStorageWorkletDriver;
class SharedStorageRuntimeManager;
class StoragePartitionImpl;
class PageImpl;

// The SharedStorageWorkletHost is responsible for getting worklet operation
// requests (i.e. `addModule()`, `selectURL()`, `run()`) from the renderer (i.e.
// document that is hosting the worklet) and running it on the
// `SharedStorageWorkletService`. It will also handle the commands from the
// `SharedStorageWorkletService` (i.e. storage access, console log) which
// could happen while running those worklet operations.
//
// The SharedStorageWorkletHost lives in the `SharedStorageRuntimeManager`,
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
      blink::mojom::SharedStorageDataOriginType data_origin_type,
      const GURL& script_source_url,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::SharedStorageWorkletCreationMethod creation_method,
      int worklet_ordinal_id,
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
      bool resolve_to_config,
      const std::u16string& saved_query_name,
      base::TimeTicks start_time,
      SelectURLCallback callback) override;
  void Run(const std::string& name,
           blink::CloneableMessage serialized_data,
           bool keep_alive_after_operation,
           blink::mojom::PrivateAggregationConfigPtr private_aggregation_config,
           base::TimeTicks start_time,
           RunCallback callback) override;

  // Whether there are unfinished worklet operations (i.e. `addModule()`,
  // `selectURL()`, or `run()`.
  bool HasPendingOperations();

  // Called by the `SharedStorageRuntimeManager` for this host to enter
  // keep-alive phase.
  void EnterKeepAliveOnDocumentDestroyed(KeepAliveFinishedCallback callback);

  // blink::mojom::SharedStorageWorkletServiceClient:
  void SharedStorageUpdate(
      network::mojom::SharedStorageModifierMethodWithOptionsPtr
          method_with_options,
      SharedStorageUpdateCallback callback) override;
  void SharedStorageBatchUpdate(
      std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
          methods_with_options,
      const std::optional<std::string>& with_lock,
      SharedStorageBatchUpdateCallback callback) override;
  void SharedStorageGet(const std::u16string& key,
                        SharedStorageGetCallback callback) override;
  void SharedStorageKeys(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener) override;
  void SharedStorageEntries(
      mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
          pending_listener,
      bool values_only) override;
  void SharedStorageLength(SharedStorageLengthCallback callback) override;
  void SharedStorageRemainingBudget(
      SharedStorageRemainingBudgetCallback callback) override;
  void GetInterestGroups(GetInterestGroupsCallback callback) override;
  void DidAddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                              const std::string& message) override;
  void RecordUseCounters(
      const std::vector<blink::mojom::WebFeature>& features) override;

  void GetLockManager(
      mojo::PendingReceiver<blink::mojom::LockManager> receiver);

  void ReportNoBinderForInterface(const std::string& error);

  // Returns the process host associated with the worklet. Returns nullptr if
  // the process has gone (e.g. during shutdown).
  RenderProcessHost* GetProcessHost() const;

  // Returns the creator frame of the worklet. Returns nullptr if the frame has
  // gone (e.g. during keep-alive phase).
  RenderFrameHostImpl* GetFrame();

  // Returns the associated main frame's GlobalRenderFrameHostId if
  // `document_service_` is still alive. Returns the default null
  // GlobalRenderFrameHostId if `document_service_` is gone (e.g. during
  // keep-alive phase).
  GlobalRenderFrameHostId GetMainFrameIdIfAvailable() const;

  const GURL& script_source_url() const {
    return script_source_url_;
  }

  blink::mojom::SharedStorageWorkletCreationMethod creation_method() const {
    return creation_method_;
  }

  const base::UnguessableToken& GetWorkletDevToolsTokenForTesting() const;

 protected:
  // virtual for testing
  virtual void OnCreateWorkletScriptLoadingFinished(
      bool success,
      const std::string& error_message);

  virtual void OnRunOperationOnWorkletFinished(
      base::TimeTicks run_start_time,
      base::TimeTicks execution_start_time,
      int operation_id,
      bool success,
      const std::string& error_message);

  virtual void OnRunURLSelectionOperationOnWorkletFinished(
      const GURL& urn_uuid,
      base::TimeTicks select_url_start_time,
      base::TimeTicks execution_start_time,
      int operation_id,
      const std::string& operation_name,
      const std::u16string& saved_query_name_to_cache,
      bool script_execution_succeeded,
      const std::string& script_execution_error_message,
      uint32_t index,
      bool use_page_budgets,
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

  void SetDataOriginOptInResultAndMaybeFinish(
      bool opted_in,
      std::string data_origin_opt_in_error_message);

  void OnOptInRequestComplete(std::unique_ptr<std::string> response_body);

  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);

  void MaybeFinishCreateWorklet();

  void OnRunURLSelectionOperationOnWorkletScriptExecutionFinished(
      const GURL& urn_uuid,
      base::TimeTicks select_url_start_time,
      base::TimeTicks execution_start_time,
      int operation_id,
      const std::string& operation_name,
      const std::u16string& saved_query_name_to_cache,
      bool success,
      const std::string& error_message,
      uint32_t index);

  void OnSelectURLSavedQueryFound(const GURL& urn_uuid,
                                  base::TimeTicks select_url_start_time,
                                  base::TimeTicks execution_start_time,
                                  int operation_id,
                                  const std::string& operation_name,
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

  // Returns `devtools_handle_->devtools_token()`.
  const base::UnguessableToken& GetWorkletDevToolsToken() const;

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

  // The owning `SharedStorageRuntimeManager`, which will outlive `this`.
  raw_ptr<SharedStorageRuntimeManager> shared_storage_runtime_manager_;

  // Pointer to the `BrowserContext`, saved to be able to call
  // `IsSharedStorageAllowed()`, and to get the global URLLoaderFactory.
  raw_ptr<BrowserContext> browser_context_;

  // Method used to create the worklet (i.e. addModule or createWorklet).
  blink::mojom::SharedStorageWorkletCreationMethod creation_method_;

  // The shared storage worklet's origin and site for data access and permission
  // checks.
  url::Origin shared_storage_origin_;
  net::SchemefulSite shared_storage_site_;

  // To avoid race conditions associated with top frame navigations and to be
  // able to call `IsSharedStorageAllowed()` during keep-alive, we need to save
  // the value of the main frame origin in the constructor.
  const url::Origin main_frame_origin_;

  // To keep track of which origin was the context origin at the time of the
  // worklet's creation (for later use in `OnJsonParsed()`).
  const url::Origin creator_context_origin_;

  // Whether `shared_storage_origin_` is same origin with the creator context's
  // origin.
  bool is_same_origin_worklet_;

  // True if `is_same_origin_worklet_` is false and `shared_storage_origin_` is
  // also cross-origin to `script_source_url_`.
  bool needs_data_origin_opt_in_;

  // Whether saved queries are supported.
  const bool saved_queries_enabled_;

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

  // Whether navigator.locks has been invoked for this worklet.
  bool navigator_locks_invoked_ = false;

  // Timer for starting and ending the keep-alive phase.
  base::OneShotTimer keep_alive_timer_;

  // Source ID of the page that spawned the worklet.
  ukm::SourceId source_id_;

  // A monotonically increasing ID assigned to each SharedStorageWorkletHost.
  // TODO(crbug.com/401011862): Use this ID in DevTools reporting for Shared
  // Storage.
  int worklet_ordinal_id_ = 0;

  // A monotonically increasing ID assigned to each run or selectURL call.
  // TODO(crbug.com/401011862): Use this ID in DevTools reporting for Shared
  // Storage.
  int next_operation_id_ = 0;

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

  // Will be assigned a value when `SetDataOriginOptInResultAndMaybeFinish()`
  // has been called, i.e. either during `OnOptInRequestComplete()` or during
  // `OnJsonParsed()`. If the /.well-known JSON file is successfully received,
  // parsed, and allows opt-in, the bool value will be true, and the string will
  // be empty. If there are any errors, or if the opt-in was denied, the bool
  // value will be false, and the string value will contain the relevant error
  // message.
  std::optional<std::pair<bool, std::string>> data_origin_opt_in_state_;

  // Will be assigned a value when `OnCreateWorkletScriptLoadingFinished()` is
  // called. If the script is successfully loaded, this will be (true, "").
  // Otherwise, it will be the pair given by false and the relevant error
  // message.
  std::optional<std::pair<bool, std::string>> script_loading_state_;

  // This will store the callback passed via mojom so that it can be called from
  // the last invocation of `MaybeFinishCreateWorklet()`.
  blink::mojom::SharedStorageDocumentService::CreateWorkletCallback
      create_worklet_finished_callback_;

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

  // URLLoaderFactory and SimpleURLLoader for making a request to data origin's
  // /.well-known/shared-storage/trusted-origins file during worklet creation to
  // check for opt-in if `needs_data_origin_opt_in_` is true.
  mojo::Remote<network::mojom::URLLoaderFactory>
      data_origin_opt_in_url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> data_origin_opt_in_url_loader_;

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
