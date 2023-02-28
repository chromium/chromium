// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/common/content_export.h"
#include "content/common/shared_storage_worklet_service.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-forward.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class SharedStorageDocumentServiceImpl;
class SharedStorageURLLoaderFactoryProxy;
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
    : public shared_storage_worklet::mojom::SharedStorageWorkletServiceClient {
 public:
  using BudgetResult = storage::SharedStorageManager::BudgetResult;

  enum class AddModuleState {
    kNotInitiated,
    kInitiated,
  };

  using KeepAliveFinishedCallback =
      base::OnceCallback<void(SharedStorageWorkletHost*)>;

  explicit SharedStorageWorkletHost(
      std::unique_ptr<SharedStorageWorkletDriver> driver,
      SharedStorageDocumentServiceImpl& document_service);
  ~SharedStorageWorkletHost() override;

  void AddModuleOnWorklet(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          frame_url_loader_factory,
      const url::Origin& frame_origin,
      const GURL& script_source_url,
      blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
          callback);
  void RunOperationOnWorklet(const std::string& name,
                             const std::vector<uint8_t>& serialized_data);
  void RunURLSelectionOperationOnWorklet(
      const std::string& name,
      std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
          urls_with_metadata,
      const std::vector<uint8_t>& serialized_data,
      blink::mojom::SharedStorageDocumentService::
          RunURLSelectionOperationOnWorkletCallback callback);

  // Whether there are unfinished worklet operations (i.e. `addModule()`,
  // `selectURL()`, or `run()`.
  bool HasPendingOperations();

  // Called by the `SharedStorageWorkletHostManager` for this host to enter
  // keep-alive phase.
  void EnterKeepAliveOnDocumentDestroyed(KeepAliveFinishedCallback callback);

  // shared_storage_worklet::mojom::SharedStorageWorkletServiceClient:
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
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener) override;
  void SharedStorageEntries(
      mojo::PendingRemote<
          shared_storage_worklet::mojom::SharedStorageEntriesListener>
          pending_listener) override;
  void SharedStorageLength(SharedStorageLengthCallback callback) override;
  void SharedStorageRemainingBudget(
      SharedStorageRemainingBudgetCallback callback) override;
  void ConsoleLog(const std::string& message) override;
  void RecordUseCounters(
      const std::vector<blink::mojom::WebFeature>& features) override;

  const url::Origin& shared_storage_origin_for_testing() const {
    return shared_storage_origin_;
  }

 protected:
  // virtual for testing
  virtual void OnAddModuleOnWorkletFinished(
      blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
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

  base::OneShotTimer& GetKeepAliveTimerForTesting() {
    return keep_alive_timer_;
  }

 private:
  void OnRunURLSelectionOperationOnWorkletScriptExecutionFinished(
      const GURL& urn_uuid,
      base::TimeTicks start_time,
      bool success,
      const std::string& error_message,
      uint32_t index);

  // Returns whether the the worklet has entered keep-alive phase. During
  // keep-alive: the attempt to log console messages will be ignored; and the
  // completion of the last pending operation will terminate the worklet.
  bool IsInKeepAlivePhase() const;

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

  shared_storage_worklet::mojom::SharedStorageWorkletService*
  GetAndConnectToSharedStorageWorkletService();

  // Binds a receiver to the `PrivateAggregationManager` and returns the
  // `PendingRemote`. If there is no `PrivateAggregationManger`, returns an
  // invalid `PendingRemote`.
  mojo::PendingRemote<content::mojom::PrivateAggregationHost>
  MaybeBindPrivateAggregationHost();

  bool IsSharedStorageAllowed();

  AddModuleState add_module_state_ = AddModuleState::kNotInitiated;

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

  // The shared storage owner document's origin.
  url::Origin shared_storage_origin_;

  // To avoid race conditions associated with top frame navigations and to be
  // able to call `IsSharedStorageAllowed()` during keep-alive, we need to save
  // the value of the main frame origin in the constructor.
  const url::Origin main_frame_origin_;

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

  // Both `shared_storage_worklet_service_`
  // and `shared_storage_worklet_service_client_` are bound in
  // GetAndConnectToSharedStorageWorkletService().
  //
  // `shared_storage_worklet_service_client_` is associated specifically with
  // the pipe that `shared_storage_worklet_service_` runs on, as we want the
  // messages initiated from the worklet (e.g. storage access, console log) to
  // be well ordered with respect to the corresponding request's callback
  // message which will be interpreted as the completion of an operation.
  mojo::Remote<shared_storage_worklet::mojom::SharedStorageWorkletService>
      shared_storage_worklet_service_;

  mojo::AssociatedReceiver<
      shared_storage_worklet::mojom::SharedStorageWorkletServiceClient>
      shared_storage_worklet_service_client_{this};

  // The proxy is used to limit the request that the worklet can make, e.g. to
  // ensure the URL is not modified by a compromised worklet; to enforce the
  // application/javascript request header; to enforce same-origin mode; etc.
  std::unique_ptr<SharedStorageURLLoaderFactoryProxy> url_loader_factory_proxy_;

  base::WeakPtrFactory<SharedStorageWorkletHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_WORKLET_HOST_H_
