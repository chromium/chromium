// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_worklet_host.h"

#include <utility>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "components/services/storage/shared_storage/public/mojom/shared_storage.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_url_loader_factory_proxy.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/common/private_aggregation_features.h"
#include "content/common/private_aggregation_host.mojom.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"

namespace content {

namespace {

using AccessType =
    SharedStorageWorkletHostManager::SharedStorageObserverInterface::AccessType;

constexpr base::TimeDelta kKeepAliveTimeout = base::Seconds(2);

using SharedStorageURNMappingResult =
    FencedFrameURLMapping::SharedStorageURNMappingResult;

using OperationResult = storage::SharedStorageManager::OperationResult;
using GetResult = storage::SharedStorageManager::GetResult;

SharedStorageURNMappingResult CreateSharedStorageURNMappingResult(
    StoragePartition* storage_partition,
    BrowserContext* browser_context,
    const url::Origin& shared_storage_origin,
    std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
        urls_with_metadata,
    uint32_t index,
    double budget_remaining,
    bool& failed_due_to_no_budget) {
  DCHECK(!failed_due_to_no_budget);
  DCHECK_GT(urls_with_metadata.size(), 0u);
  DCHECK_LT(index, urls_with_metadata.size());

  double budget_to_charge = std::log2(urls_with_metadata.size());

  // If we are running out of budget, consider this mapping to be failed. Use
  // the default URL, and there's no need to further charge the budget.
  if (budget_to_charge > 0.0 && budget_to_charge > budget_remaining) {
    failed_due_to_no_budget = true;
    index = 0;
    budget_to_charge = 0.0;
  }

  GURL mapped_url = urls_with_metadata[index]->url;

  scoped_refptr<FencedFrameReporter> fenced_frame_reporter;
  if (!urls_with_metadata[index]->reporting_metadata.empty()) {
    fenced_frame_reporter = FencedFrameReporter::CreateForSharedStorage(
        storage_partition->GetURLLoaderFactoryForBrowserProcess(),
        AttributionDataHostManager::FromBrowserContext(browser_context),
        urls_with_metadata[index]->reporting_metadata);
  }
  return SharedStorageURNMappingResult(
      mapped_url,
      SharedStorageBudgetMetadata{.origin = shared_storage_origin,
                                  .budget_to_charge = budget_to_charge},
      std::move(fenced_frame_reporter));
}

}  // namespace

SharedStorageWorkletHost::SharedStorageWorkletHost(
    std::unique_ptr<SharedStorageWorkletDriver> driver,
    SharedStorageDocumentServiceImpl& document_service)
    : driver_(std::move(driver)),
      document_service_(document_service.GetWeakPtr()),
      page_(
          static_cast<PageImpl&>(document_service.render_frame_host().GetPage())
              .GetWeakPtrImpl()),
      storage_partition_(static_cast<StoragePartitionImpl*>(
          document_service.render_frame_host().GetStoragePartition())),
      shared_storage_manager_(storage_partition_->GetSharedStorageManager()),
      shared_storage_worklet_host_manager_(
          storage_partition_->GetSharedStorageWorkletHostManager()),
      browser_context_(
          document_service.render_frame_host().GetBrowserContext()),
      shared_storage_origin_(
          document_service.render_frame_host().GetLastCommittedOrigin()),
      main_frame_origin_(document_service.main_frame_origin()),
      creation_time_(base::TimeTicks::Now()) {
  GetContentClient()->browser()->OnSharedStorageWorkletHostCreated(
      &(document_service.render_frame_host()));
}

SharedStorageWorkletHost::~SharedStorageWorkletHost() {
  base::UmaHistogramEnumeration("Storage.SharedStorage.Worklet.DestroyedStatus",
                                destroyed_status_);

  base::TimeDelta elapsed_time_since_creation =
      base::TimeTicks::Now() - creation_time_;
  if (pending_operations_count_ > 0 ||
      last_operation_finished_time_.is_null() ||
      elapsed_time_since_creation.is_zero()) {
    base::UmaHistogramCounts100(
        "Storage.SharedStorage.Worklet.Timing.UsefulResourceDuration", 100);
  } else {
    base::UmaHistogramCounts100(
        "Storage.SharedStorage.Worklet.Timing.UsefulResourceDuration",
        100 * (last_operation_finished_time_ - creation_time_) /
            elapsed_time_since_creation);
  }

  if (!page_)
    return;

  // If the worklet is destructed and there are still unresolved URNs (i.e. the
  // keep-alive timeout is reached), consider the mapping to be failed.
  auto it = unresolved_urns_.begin();
  while (it != unresolved_urns_.end()) {
    const GURL& urn_uuid = it->first;

    bool failed_due_to_no_budget = false;
    absl::optional<FencedFrameConfig> config =
        page_->fenced_frame_urls_map()
            .OnSharedStorageURNMappingResultDetermined(
                urn_uuid, CreateSharedStorageURNMappingResult(
                              storage_partition_, browser_context_,
                              shared_storage_origin_, std::move(it->second),
                              /*index=*/0, /*budget_remaining=*/0.0,
                              failed_due_to_no_budget));

    shared_storage_worklet_host_manager_->NotifyConfigPopulated(config);

    it = unresolved_urns_.erase(it);
  }
}

void SharedStorageWorkletHost::AddModuleOnWorklet(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        frame_url_loader_factory,
    const url::Origin& frame_origin,
    const GURL& script_source_url,
    blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
        callback) {
  // This function is invoked from `document_service_`. Thus both `page_` and
  // `document_service_` should be valid.
  DCHECK(page_);
  DCHECK(document_service_);
  IncrementPendingOperationsCount();

  if (add_module_state_ == AddModuleState::kInitiated) {
    OnAddModuleOnWorkletFinished(
        std::move(callback), /*success=*/false,
        /*error_message=*/
        "sharedStorage.worklet.addModule() can only be "
        "invoked once per browsing context.");
    return;
  }

  add_module_state_ = AddModuleState::kInitiated;

  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;

  url_loader_factory_proxy_ =
      std::make_unique<SharedStorageURLLoaderFactoryProxy>(
          std::move(frame_url_loader_factory),
          url_loader_factory.InitWithNewPipeAndPassReceiver(), frame_origin,
          script_source_url);

  GetAndConnectToSharedStorageWorkletService()->AddModule(
      std::move(url_loader_factory), script_source_url,
      base::BindOnce(&SharedStorageWorkletHost::OnAddModuleOnWorkletFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SharedStorageWorkletHost::RunOperationOnWorklet(
    const std::string& name,
    const std::vector<uint8_t>& serialized_data) {
  // This function is invoked from `document_service_`. Thus both `page_` and
  // `document_service_` should be valid.
  DCHECK(page_);
  DCHECK(document_service_);
  IncrementPendingOperationsCount();

  if (add_module_state_ != AddModuleState::kInitiated) {
    OnRunOperationOnWorkletFinished(
        base::TimeTicks::Now(),
        /*success=*/false,
        /*error_message=*/
        "sharedStorage.worklet.addModule() has to be called before "
        "sharedStorage.run().");
    return;
  }

  GetAndConnectToSharedStorageWorkletService()->RunOperation(
      name, serialized_data,
      base::BindOnce(&SharedStorageWorkletHost::OnRunOperationOnWorkletFinished,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void SharedStorageWorkletHost::RunURLSelectionOperationOnWorklet(
    const std::string& name,
    std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
        urls_with_metadata,
    const std::vector<uint8_t>& serialized_data,
    blink::mojom::SharedStorageDocumentService::
        RunURLSelectionOperationOnWorkletCallback callback) {
  if (add_module_state_ != AddModuleState::kInitiated) {
    std::move(callback).Run(
        /*success=*/false, /*error_message=*/
        "sharedStorage.worklet.addModule() has to be called before "
        "sharedStorage.selectURL().",
        /*result_config=*/absl::nullopt);
    return;
  }

  // This function is invoked from `document_service_`. Thus both `page_` and
  // `document_service_` should be valid.
  DCHECK(page_);
  DCHECK(document_service_);

  auto pending_urn_uuid =
      page_->fenced_frame_urls_map().GeneratePendingMappedURN();

  if (!pending_urn_uuid.has_value()) {
    // Pending urn::uuid cannot be inserted to pending urn map because number of
    // urn mappings has reached limit.
    std::move(callback).Run(
        /*success=*/false, /*error_message=*/
        "sharedStorage.selectURL() failed because number of urn::uuid to url "
        "mappings has reached the limit.",
        /*result_config=*/absl::nullopt);
    return;
  }

  GURL urn_uuid = pending_urn_uuid.value();
  IncrementPendingOperationsCount();

  std::vector<GURL> urls;
  for (const auto& url_with_metadata : urls_with_metadata)
    urls.emplace_back(url_with_metadata->url);

  bool emplace_succeeded =
      unresolved_urns_.emplace(urn_uuid, std::move(urls_with_metadata)).second;

  // Assert that `urn_uuid` was not in the set before.
  DCHECK(emplace_succeeded);

  FencedFrameConfig config;
  config.urn_uuid_ = absl::make_optional(urn_uuid);
  std::move(callback).Run(
      /*success=*/true, /*error_message=*/{},
      /*result_config=*/
      config.RedactFor(FencedFrameEntity::kEmbedder));

  shared_storage_worklet_host_manager_->NotifyUrnUuidGenerated(urn_uuid);

  GetAndConnectToSharedStorageWorkletService()->RunURLSelectionOperation(
      name, urls, serialized_data,
      base::BindOnce(
          &SharedStorageWorkletHost::
              OnRunURLSelectionOperationOnWorkletScriptExecutionFinished,
          weak_ptr_factory_.GetWeakPtr(), urn_uuid, base::TimeTicks::Now()));
}

bool SharedStorageWorkletHost::HasPendingOperations() {
  return pending_operations_count_ > 0;
}

void SharedStorageWorkletHost::EnterKeepAliveOnDocumentDestroyed(
    KeepAliveFinishedCallback callback) {
  // At this point the `SharedStorageDocumentServiceImpl` is being destroyed, so
  // `document_service_` is still valid. But it will be auto reset soon after.
  DCHECK(document_service_);
  DCHECK(HasPendingOperations());
  DCHECK(keep_alive_finished_callback_.is_null());

  keep_alive_finished_callback_ = std::move(callback);

  keep_alive_timer_.Start(
      FROM_HERE, GetKeepAliveTimeout(),
      base::BindOnce(&SharedStorageWorkletHost::FinishKeepAlive,
                     weak_ptr_factory_.GetWeakPtr(), /*timeout_reached=*/true));

  enter_keep_alive_time_ = base::TimeTicks::Now();
  destroyed_status_ = blink::SharedStorageWorkletDestroyedStatus::kOther;
}

void SharedStorageWorkletHost::SharedStorageSet(
    const std::u16string& key,
    const std::u16string& value,
    bool ignore_if_present,
    SharedStorageSetCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageDisabledMessage);
    return;
  }

  if (document_service_) {
    shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
        AccessType::kWorkletSet, document_service_->main_frame_id(),
        shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateForSet(base::UTF16ToUTF8(key),
                                               base::UTF16ToUTF8(value),
                                               ignore_if_present));
  }

  auto operation_completed_callback = base::BindOnce(
      [](SharedStorageSetCallback callback, OperationResult result) {
        if (result != OperationResult::kSet &&
            result != OperationResult::kIgnored) {
          std::move(callback).Run(
              /*success=*/false,
              /*error_message=*/"sharedStorage.set() failed");
          return;
        }

        std::move(callback).Run(
            /*success=*/true,
            /*error_message=*/{});
      },
      std::move(callback));

  storage::SharedStorageDatabase::SetBehavior set_behavior =
      ignore_if_present
          ? storage::SharedStorageDatabase::SetBehavior::kIgnoreIfPresent
          : storage::SharedStorageDatabase::SetBehavior::kDefault;

  shared_storage_manager_->Set(shared_storage_origin_, key, value,
                               std::move(operation_completed_callback),
                               set_behavior);
}

void SharedStorageWorkletHost::SharedStorageAppend(
    const std::u16string& key,
    const std::u16string& value,
    SharedStorageAppendCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageDisabledMessage);
    return;
  }

  if (document_service_) {
    shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
        AccessType::kWorkletAppend, document_service_->main_frame_id(),
        shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateForAppend(base::UTF16ToUTF8(key),
                                                  base::UTF16ToUTF8(value)));
  }

  auto operation_completed_callback = base::BindOnce(
      [](SharedStorageAppendCallback callback, OperationResult result) {
        if (result != OperationResult::kSet) {
          std::move(callback).Run(
              /*success=*/false,
              /*error_message=*/"sharedStorage.append() failed");
          return;
        }

        std::move(callback).Run(
            /*success=*/true,
            /*error_message=*/{});
      },
      std::move(callback));

  shared_storage_manager_->Append(shared_storage_origin_, key, value,
                                  std::move(operation_completed_callback));
}

void SharedStorageWorkletHost::SharedStorageDelete(
    const std::u16string& key,
    SharedStorageDeleteCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageDisabledMessage);
    return;
  }

  if (document_service_) {
    shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
        AccessType::kWorkletDelete, document_service_->main_frame_id(),
        shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateForGetOrDelete(base::UTF16ToUTF8(key)));
  }

  auto operation_completed_callback = base::BindOnce(
      [](SharedStorageDeleteCallback callback, OperationResult result) {
        if (result != OperationResult::kSuccess) {
          std::move(callback).Run(
              /*success=*/false,
              /*error_message=*/"sharedStorage.delete() failed");
          return;
        }

        std::move(callback).Run(
            /*success=*/true,
            /*error_message=*/{});
      },
      std::move(callback));

  shared_storage_manager_->Delete(shared_storage_origin_, key,
                                  std::move(operation_completed_callback));
}

void SharedStorageWorkletHost::SharedStorageClear(
    SharedStorageClearCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageDisabledMessage);
    return;
  }

  if (document_service_) {
    shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
        AccessType::kWorkletClear, document_service_->main_frame_id(),
        shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateDefault());
  }

  auto operation_completed_callback = base::BindOnce(
      [](SharedStorageClearCallback callback, OperationResult result) {
        if (result != OperationResult::kSuccess) {
          std::move(callback).Run(
              /*success=*/false,
              /*error_message=*/"sharedStorage.clear() failed");
          return;
        }

        std::move(callback).Run(
            /*success=*/true,
            /*error_message=*/{});
      },
      std::move(callback));

  shared_storage_manager_->Clear(shared_storage_origin_,
                                 std::move(operation_completed_callback));
}

void SharedStorageWorkletHost::SharedStorageGet(
    const std::u16string& key,
    SharedStorageGetCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        shared_storage_worklet::mojom::SharedStorageGetStatus::kError,
        /*error_message=*/kSharedStorageDisabledMessage, /*value=*/{});
    return;
  }

  if (document_service_) {
    shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
        AccessType::kWorkletGet, document_service_->main_frame_id(),
        shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateForGetOrDelete(base::UTF16ToUTF8(key)));
  }

  auto operation_completed_callback = base::BindOnce(
      [](SharedStorageGetCallback callback, GetResult result) {
        // If the key is not found but there is no other error, the worklet will
        // resolve the promise to undefined.
        if (result.result == OperationResult::kNotFound ||
            result.result == OperationResult::kExpired) {
          std::move(callback).Run(
              shared_storage_worklet::mojom::SharedStorageGetStatus::kNotFound,
              /*error_message=*/"sharedStorage.get() could not find key",
              /*value=*/{});
          return;
        }

        if (result.result != OperationResult::kSuccess) {
          std::move(callback).Run(
              shared_storage_worklet::mojom::SharedStorageGetStatus::kError,
              /*error_message=*/"sharedStorage.get() failed", /*value=*/{});
          return;
        }

        std::move(callback).Run(
            shared_storage_worklet::mojom::SharedStorageGetStatus::kSuccess,
            /*error_message=*/{}, /*value=*/result.data);
      },
      std::move(callback));

  shared_storage_manager_->Get(shared_storage_origin_, key,
                               std::move(operation_completed_callback));
}

void SharedStorageWorkletHost::SharedStorageKeys(
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  if (!IsSharedStorageAllowed()) {
    mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
        listener(std::move(pending_listener));
    listener->DidReadEntries(
        /*success=*/false, kSharedStorageDisabledMessage,
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return;
  }

  if (document_service_) {
    shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
        AccessType::kWorkletKeys, document_service_->main_frame_id(),
        shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateDefault());
  }

  shared_storage_manager_->Keys(shared_storage_origin_,
                                std::move(pending_listener), base::DoNothing());
}

void SharedStorageWorkletHost::SharedStorageEntries(
    mojo::PendingRemote<
        shared_storage_worklet::mojom::SharedStorageEntriesListener>
        pending_listener) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  if (!IsSharedStorageAllowed()) {
    mojo::Remote<shared_storage_worklet::mojom::SharedStorageEntriesListener>
        listener(std::move(pending_listener));
    listener->DidReadEntries(
        /*success=*/false, kSharedStorageDisabledMessage,
        /*entries=*/{}, /*has_more_entries=*/false, /*total_queued_to_send=*/0);
    return;
  }

  if (document_service_) {
    shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
        AccessType::kWorkletEntries, document_service_->main_frame_id(),
        shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateDefault());
  }

  shared_storage_manager_->Entries(
      shared_storage_origin_, std::move(pending_listener), base::DoNothing());
}

void SharedStorageWorkletHost::SharedStorageLength(
    SharedStorageLengthCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageDisabledMessage, /*length=*/0);
    return;
  }

  if (document_service_) {
    shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
        AccessType::kWorkletLength, document_service_->main_frame_id(),
        shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateDefault());
  }

  auto operation_completed_callback = base::BindOnce(
      [](SharedStorageLengthCallback callback, int result) {
        if (result == -1) {
          std::move(callback).Run(
              /*success=*/false,
              /*error_message=*/"sharedStorage.length() failed", /*length=*/0);
          return;
        }

        DCHECK_GE(result, 0);

        std::move(callback).Run(
            /*success=*/true,
            /*error_message=*/{},
            /*length=*/result);
      },
      std::move(callback));

  shared_storage_manager_->Length(shared_storage_origin_,
                                  std::move(operation_completed_callback));
}

void SharedStorageWorkletHost::SharedStorageRemainingBudget(
    SharedStorageRemainingBudgetCallback callback) {
  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  if (!IsSharedStorageAllowed()) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/kSharedStorageDisabledMessage, /*bits=*/0.0);
    return;
  }

  if (document_service_) {
    shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
        AccessType::kWorkletRemainingBudget, document_service_->main_frame_id(),
        shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateDefault());
  }

  auto operation_completed_callback = base::BindOnce(
      [](SharedStorageRemainingBudgetCallback callback, BudgetResult result) {
        if (result.result != OperationResult::kSuccess) {
          std::move(callback).Run(
              /*success=*/false,
              /*error_message=*/"sharedStorage.remainingBudget() failed",
              /*bits=*/0.0);
          return;
        }

        std::move(callback).Run(
            /*success=*/true,
            /*error_message=*/{},
            /*bits=*/result.bits);
      },
      std::move(callback));

  shared_storage_manager_->GetRemainingBudget(
      shared_storage_origin_, std::move(operation_completed_callback));
}

void SharedStorageWorkletHost::ConsoleLog(const std::string& message) {
  if (!document_service_) {
    DCHECK(IsInKeepAlivePhase());
    return;
  }

  DCHECK(add_module_state_ == AddModuleState::kInitiated);

  devtools_instrumentation::LogWorkletMessage(
      static_cast<RenderFrameHostImpl&>(document_service_->render_frame_host()),
      blink::mojom::ConsoleMessageLevel::kInfo, message);
}

void SharedStorageWorkletHost::RecordUseCounters(
    const std::vector<blink::mojom::WebFeature>& features) {
  // If the worklet host has outlived the page, we unfortunately can't count the
  // feature.
  if (!page_)
    return;

  for (blink::mojom::WebFeature feature : features) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &page_->GetMainDocument(), feature);
  }
}

void SharedStorageWorkletHost::OnAddModuleOnWorkletFinished(
    blink::mojom::SharedStorageDocumentService::AddModuleOnWorkletCallback
        callback,
    bool success,
    const std::string& error_message) {
  std::move(callback).Run(success, error_message);

  DecrementPendingOperationsCount();
}

void SharedStorageWorkletHost::OnRunOperationOnWorkletFinished(
    base::TimeTicks start_time,
    bool success,
    const std::string& error_message) {
  if (!success) {
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kRunNonWebVisible);
    if (document_service_) {
      DCHECK(!IsInKeepAlivePhase());
      devtools_instrumentation::LogWorkletMessage(
          static_cast<RenderFrameHostImpl&>(
              document_service_->render_frame_host()),
          blink::mojom::ConsoleMessageLevel::kError, error_message);
    }
  }

  base::UmaHistogramLongTimes(
      "Storage.SharedStorage.Document.Timing.Run.ExecutedInWorklet",
      base::TimeTicks::Now() - start_time);
  DecrementPendingOperationsCount();
}

void SharedStorageWorkletHost::
    OnRunURLSelectionOperationOnWorkletScriptExecutionFinished(
        const GURL& urn_uuid,
        base::TimeTicks start_time,
        bool success,
        const std::string& error_message,
        uint32_t index) {
  auto it = unresolved_urns_.find(urn_uuid);
  DCHECK(it != unresolved_urns_.end());

  if ((success && index >= it->second.size()) || (!success && index != 0)) {
    // This could indicate a compromised worklet environment, so let's terminate
    // it.
    shared_storage_worklet_service_client_.ReportBadMessage(
        "Unexpected index number returned from selectURL().");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible);

    unresolved_urns_.erase(it);
    DecrementPendingOperationsCount();
    return;
  }

  shared_storage_manager_->GetRemainingBudget(
      shared_storage_origin_,
      base::BindOnce(&SharedStorageWorkletHost::
                         OnRunURLSelectionOperationOnWorkletFinished,
                     weak_ptr_factory_.GetWeakPtr(), urn_uuid, start_time,
                     success, error_message, index));
}

void SharedStorageWorkletHost::OnRunURLSelectionOperationOnWorkletFinished(
    const GURL& urn_uuid,
    base::TimeTicks start_time,
    bool script_execution_succeeded,
    const std::string& script_execution_error_message,
    uint32_t index,
    BudgetResult budget_result) {
  auto it = unresolved_urns_.find(urn_uuid);
  DCHECK(it != unresolved_urns_.end());

  std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
      urls_with_metadata = std::move(it->second);
  unresolved_urns_.erase(it);

  if (page_) {
    bool failed_due_to_no_budget = false;
    SharedStorageURNMappingResult mapping_result =
        CreateSharedStorageURNMappingResult(
            storage_partition_, browser_context_, shared_storage_origin_,
            std::move(urls_with_metadata), index, budget_result.bits,
            failed_due_to_no_budget);

    if (document_service_) {
      DCHECK(!IsInKeepAlivePhase());

      // Let the insufficient-budget failure supersede the script failure.
      if (failed_due_to_no_budget) {
        devtools_instrumentation::LogWorkletMessage(
            static_cast<RenderFrameHostImpl&>(
                document_service_->render_frame_host()),
            blink::mojom::ConsoleMessageLevel::kError,
            "Insufficient budget for selectURL().");
        LogSharedStorageWorkletError(
            blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible);
      } else if (!script_execution_succeeded) {
        devtools_instrumentation::LogWorkletMessage(
            static_cast<RenderFrameHostImpl&>(
                document_service_->render_frame_host()),
            blink::mojom::ConsoleMessageLevel::kError,
            script_execution_error_message);
        LogSharedStorageWorkletError(
            blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisible);
      }
    }

    absl::optional<FencedFrameConfig> config =
        page_->fenced_frame_urls_map()
            .OnSharedStorageURNMappingResultDetermined(
                urn_uuid, std::move(mapping_result));

    shared_storage_worklet_host_manager_->NotifyConfigPopulated(config);
  }

  base::UmaHistogramLongTimes(
      "Storage.SharedStorage.Document.Timing.SelectURL.ExecutedInWorklet",
      base::TimeTicks::Now() - start_time);
  DecrementPendingOperationsCount();
}

bool SharedStorageWorkletHost::IsInKeepAlivePhase() const {
  return !!keep_alive_finished_callback_;
}

void SharedStorageWorkletHost::FinishKeepAlive(bool timeout_reached) {
  if (timeout_reached) {
    destroyed_status_ =
        blink::SharedStorageWorkletDestroyedStatus::kKeepAliveEndedDueToTimeout;
  } else {
    destroyed_status_ = blink::SharedStorageWorkletDestroyedStatus::
        kKeepAliveEndedDueToOperationsFinished;
    DCHECK(!enter_keep_alive_time_.is_null());
    base::UmaHistogramTimes(
        "Storage.SharedStorage.Worklet.Timing."
        "KeepAliveEndedDueToOperationsFinished.KeepAliveDuration",
        base::TimeTicks::Now() - enter_keep_alive_time_);
  }

  // This will remove this worklet host from the manager.
  std::move(keep_alive_finished_callback_).Run(this);

  // Do not add code after this. SharedStorageWorkletHost has been destroyed.
}

void SharedStorageWorkletHost::IncrementPendingOperationsCount() {
  base::CheckedNumeric<uint32_t> count = pending_operations_count_;
  pending_operations_count_ = (++count).ValueOrDie();
}

void SharedStorageWorkletHost::DecrementPendingOperationsCount() {
  base::CheckedNumeric<uint32_t> count = pending_operations_count_;
  pending_operations_count_ = (--count).ValueOrDie();

  if (pending_operations_count_)
    return;

  // This time will be overridden if another operation is subsequently queued
  // and completed.
  last_operation_finished_time_ = base::TimeTicks::Now();

  if (!IsInKeepAlivePhase())
    return;

  FinishKeepAlive(/*timeout_reached=*/false);
}

base::TimeDelta SharedStorageWorkletHost::GetKeepAliveTimeout() const {
  return kKeepAliveTimeout;
}

shared_storage_worklet::mojom::SharedStorageWorkletService*
SharedStorageWorkletHost::GetAndConnectToSharedStorageWorkletService() {
  DCHECK(document_service_);

  if (!shared_storage_worklet_service_) {
    bool private_aggregation_permissions_policy_allowed =
        document_service_->render_frame_host().IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::kPrivateAggregation);

    driver_->StartWorkletService(
        shared_storage_worklet_service_.BindNewPipeAndPassReceiver());

    shared_storage_worklet_service_->Initialize(
        shared_storage_worklet_service_client_.BindNewEndpointAndPassRemote(),
        private_aggregation_permissions_policy_allowed,
        MaybeBindPrivateAggregationHost());
  }

  return shared_storage_worklet_service_.get();
}

mojo::PendingRemote<content::mojom::PrivateAggregationHost>
SharedStorageWorkletHost::MaybeBindPrivateAggregationHost() {
  DCHECK(browser_context_);

  if (!base::FeatureList::IsEnabled(content::kPrivateAggregationApi) ||
      !content::kPrivateAggregationApiEnabledInSharedStorage.Get()) {
    return mojo::PendingRemote<content::mojom::PrivateAggregationHost>();
  }

  PrivateAggregationManager* private_aggregation_manager =
      PrivateAggregationManager::GetManager(*browser_context_);
  DCHECK(private_aggregation_manager);

  mojo::PendingRemote<content::mojom::PrivateAggregationHost>
      pending_pa_host_remote;
  if (!private_aggregation_manager->BindNewReceiver(
          shared_storage_origin_, main_frame_origin_,
          PrivateAggregationBudgetKey::Api::kSharedStorage,
          pending_pa_host_remote.InitWithNewPipeAndPassReceiver())) {
    return mojo::PendingRemote<content::mojom::PrivateAggregationHost>();
  }

  return pending_pa_host_remote;
}

bool SharedStorageWorkletHost::IsSharedStorageAllowed() {
  RenderFrameHost* rfh =
      document_service_ ? &(document_service_->render_frame_host()) : nullptr;
  return GetContentClient()->browser()->IsSharedStorageAllowed(
      browser_context_, rfh, main_frame_origin_, shared_storage_origin_);
}

}  // namespace content
