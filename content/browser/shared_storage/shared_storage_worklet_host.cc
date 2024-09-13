// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_worklet_host.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/shared_storage_worklet_devtools_manager.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_code_cache_host_proxy.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_render_thread_worklet_driver.h"
#include "content/browser/shared_storage/shared_storage_url_loader_factory_proxy.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"
#include "content/browser/shared_storage/shared_storage_worklet_host_manager.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/blob/blob_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

using AccessType =
    SharedStorageWorkletHostManager::SharedStorageObserverInterface::AccessType;

constexpr base::TimeDelta kKeepAliveTimeout = base::Seconds(2);

using SharedStorageURNMappingResult =
    FencedFrameURLMapping::SharedStorageURNMappingResult;

using OperationResult = storage::SharedStorageManager::OperationResult;
using GetResult = storage::SharedStorageManager::GetResult;

void LogSharedStorageWorkletErrorFromErrorMessage(
    bool from_select_url,
    const std::string& error_message) {
  if (error_message == blink::kSharedStorageModuleScriptNotLoadedErrorMessage) {
    LogSharedStorageWorkletError(
        from_select_url ? blink::SharedStorageWorkletErrorType::
                              kSelectURLNonWebVisibleModuleScriptNotLoaded
                        : blink::SharedStorageWorkletErrorType::
                              kRunNonWebVisibleModuleScriptNotLoaded);
  } else if (error_message ==
             blink::kSharedStorageOperationNotFoundErrorMessage) {
    LogSharedStorageWorkletError(
        from_select_url ? blink::SharedStorageWorkletErrorType::
                              kSelectURLNonWebVisibleOperationNotFound
                        : blink::SharedStorageWorkletErrorType::
                              kRunNonWebVisibleOperationNotFound);
  } else if (error_message ==
             blink::
                 kSharedStorageEmptyOperationDefinitionInstanceErrorMessage) {
    LogSharedStorageWorkletError(
        from_select_url
            ? blink::SharedStorageWorkletErrorType::
                  kSelectURLNonWebVisibleEmptyOperationDefinitionInstance
            : blink::SharedStorageWorkletErrorType::
                  kRunNonWebVisibleEmptyOperationDefinitionInstance);
  } else if (error_message ==
             blink::kSharedStorageCannotDeserializeDataErrorMessage) {
    LogSharedStorageWorkletError(
        from_select_url ? blink::SharedStorageWorkletErrorType::
                              kSelectURLNonWebVisibleCannotDeserializeData
                        : blink::SharedStorageWorkletErrorType::
                              kRunNonWebVisibleCannotDeserializeData);
  } else if (error_message ==
             blink::kSharedStorageEmptyScriptResultErrorMessage) {
    LogSharedStorageWorkletError(
        from_select_url ? blink::SharedStorageWorkletErrorType::
                              kSelectURLNonWebVisibleEmptyScriptResult
                        : blink::SharedStorageWorkletErrorType::
                              kRunNonWebVisibleEmptyScriptResult);
  } else if (error_message ==
                 blink::kSharedStorageReturnValueToIntErrorMessage &&
             from_select_url) {
    LogSharedStorageWorkletError(blink::SharedStorageWorkletErrorType::
                                     kSelectURLNonWebVisibleReturnValueToInt);
  } else if (error_message ==
                 blink::kSharedStorageReturnValueOutOfRangeErrorMessage &&
             from_select_url) {
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kSelectURLNonWebVisibleReturnValueOutOfRange);
  } else {
    LogSharedStorageWorkletError(
        from_select_url
            ? blink::SharedStorageWorkletErrorType::kSelectURLNonWebVisibleOther
            : blink::SharedStorageWorkletErrorType::kRunNonWebVisibleOther);
  }
}

SharedStorageURNMappingResult CreateSharedStorageURNMappingResult(
    StoragePartition* storage_partition,
    BrowserContext* browser_context,
    PageImpl* page,
    const url::Origin& main_frame_origin,
    const url::Origin& shared_storage_origin,
    const net::SchemefulSite& shared_storage_site,
    std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
        urls_with_metadata,
    uint32_t index,
    double budget_remaining,
    blink::SharedStorageSelectUrlBudgetStatus& budget_status) {
  DCHECK_EQ(budget_status, blink::SharedStorageSelectUrlBudgetStatus::kOther);
  DCHECK_GT(urls_with_metadata.size(), 0u);
  DCHECK_LT(index, urls_with_metadata.size());
  DCHECK(page);

  double budget_to_charge = std::log2(urls_with_metadata.size());

  // If we are running out of budget, consider this mapping to be failed. Use
  // the default URL, and there's no need to further charge the budget.
  if (budget_to_charge > 0.0) {
    budget_status = (budget_to_charge > budget_remaining)
                        ? blink::SharedStorageSelectUrlBudgetStatus::
                              kInsufficientSiteNavigationBudget
                        : page->CheckAndMaybeDebitSelectURLBudgets(
                              shared_storage_site, budget_to_charge);
    if (budget_status !=
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget) {
      index = 0;
      budget_to_charge = 0.0;
    }
  } else {
    budget_status =
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget;
  }

  GURL mapped_url = urls_with_metadata[index]->url;

  scoped_refptr<FencedFrameReporter> fenced_frame_reporter;
  if (!urls_with_metadata[index]->reporting_metadata.empty()) {
    fenced_frame_reporter = FencedFrameReporter::CreateForSharedStorage(
        storage_partition->GetURLLoaderFactoryForBrowserProcess(),
        browser_context, shared_storage_origin,
        urls_with_metadata[index]->reporting_metadata, main_frame_origin);
  }
  return SharedStorageURNMappingResult(
      mapped_url,
      SharedStorageBudgetMetadata{.site = shared_storage_site,
                                  .budget_to_charge = budget_to_charge},
      std::move(fenced_frame_reporter));
}

// TODO(crbug.com/40847123): Consider moving this function to
// third_party/blink/common/fenced_frame/fenced_frame_utils.cc.
bool IsValidFencedFrameReportingURL(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  return url.SchemeIs(url::kHttpsScheme);
}

}  // namespace

class SharedStorageWorkletHost::ScopedDevToolsHandle
    : blink::mojom::WorkletDevToolsHost {
 public:
  explicit ScopedDevToolsHandle(SharedStorageWorkletHost& owner)
      : owner_(owner), devtools_token_(base::UnguessableToken::Create()) {
    SharedStorageWorkletDevToolsManager::GetInstance()->WorkletCreated(
        owner, devtools_token_, wait_for_debugger_);
  }

  ScopedDevToolsHandle(const ScopedDevToolsHandle&) = delete;
  ScopedDevToolsHandle& operator=(const ScopedDevToolsHandle&) = delete;

  ~ScopedDevToolsHandle() override {
    SharedStorageWorkletDevToolsManager::GetInstance()->WorkletDestroyed(
        *owner_);
  }

  // blink::mojom::WorkletDevToolsHost:
  void OnReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>
          agent_host_receiver) override {
    SharedStorageWorkletDevToolsManager::GetInstance()
        ->WorkletReadyForInspection(*owner_, std::move(agent_remote),
                                    std::move(agent_host_receiver));
  }

  const base::UnguessableToken& devtools_token() const {
    return devtools_token_;
  }

  bool wait_for_debugger() const { return wait_for_debugger_; }

  mojo::PendingRemote<blink::mojom::WorkletDevToolsHost>
  BindNewPipeAndPassRemote() {
    return host_.BindNewPipeAndPassRemote();
  }

 private:
  raw_ref<SharedStorageWorkletHost> owner_;

  mojo::Receiver<blink::mojom::WorkletDevToolsHost> host_{this};

  bool wait_for_debugger_ = false;

  const base::UnguessableToken devtools_token_;
};

SharedStorageWorkletHost::SharedStorageWorkletHost(
    SharedStorageDocumentServiceImpl& document_service,
    const url::Origin& frame_origin,
    const url::Origin& data_origin,
    const GURL& script_source_url,
    network::mojom::CredentialsMode credentials_mode,
    const std::vector<blink::mojom::OriginTrialFeature>& origin_trial_features,
    mojo::PendingAssociatedReceiver<blink::mojom::SharedStorageWorkletHost>
        worklet_host,
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback callback)
    : driver_(std::make_unique<SharedStorageRenderThreadWorkletDriver>(
          document_service.render_frame_host(),
          data_origin)),
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
      shared_storage_origin_(data_origin),
      shared_storage_site_(net::SchemefulSite(shared_storage_origin_)),
      main_frame_origin_(document_service.main_frame_origin()),
      is_same_origin_worklet_(document_service.render_frame_host()
                                  .GetLastCommittedOrigin()
                                  .IsSameOriginWith(shared_storage_origin_)),
      creation_time_(base::TimeTicks::Now()) {
  GetContentClient()->browser()->OnSharedStorageWorkletHostCreated(
      &(document_service.render_frame_host()));

  receiver_.Bind(std::move(worklet_host));

  // This is invoked from `document_service_`. Thus both `page_` and
  // `document_service_` should be valid.
  DCHECK(page_);
  DCHECK(document_service_);
  IncrementPendingOperationsCount();

  script_source_url_ = script_source_url;
  origin_trial_features_ = origin_trial_features;

  devtools_handle_ = std::make_unique<ScopedDevToolsHandle>(*this);

  // Initialize the `URLLoaderFactory` now, as later on the worklet may enter
  // keep-alive phase and won't have access to the `RenderFrameHost`.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      frame_url_loader_factory;
  if (script_source_url.SchemeIsBlob()) {
    storage::BlobURLLoaderFactory::Create(
        static_cast<StoragePartitionImpl*>(
            document_service_->render_frame_host()
                .GetProcess()
                ->GetStoragePartition())
            ->GetBlobUrlRegistry()
            ->GetBlobFromUrl(script_source_url),
        script_source_url,
        frame_url_loader_factory.InitWithNewPipeAndPassReceiver());
  } else {
    document_service_->render_frame_host().CreateNetworkServiceDefaultFactory(
        frame_url_loader_factory.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;

  // The data origin can't be opaque.
  DCHECK(!shared_storage_origin_.opaque());

  url_loader_factory_proxy_ =
      std::make_unique<SharedStorageURLLoaderFactoryProxy>(
          std::move(frame_url_loader_factory),
          url_loader_factory.InitWithNewPipeAndPassReceiver(), frame_origin,
          shared_storage_origin_, script_source_url, credentials_mode,
          static_cast<RenderFrameHostImpl&>(
              document_service_->render_frame_host())
              .ComputeSiteForCookies());

  shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
      AccessType::kDocumentAddModule, document_service_->main_frame_id(),
      shared_storage_origin_.Serialize(),
      SharedStorageEventParams::CreateForAddModule(script_source_url));

  GetAndConnectToSharedStorageWorkletService()->AddModule(
      std::move(url_loader_factory), script_source_url,
      base::BindOnce(&SharedStorageWorkletHost::OnAddModuleOnWorkletFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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

    blink::SharedStorageSelectUrlBudgetStatus budget_status =
        blink::SharedStorageSelectUrlBudgetStatus::kOther;

    std::optional<FencedFrameConfig> config =
        page_->fenced_frame_urls_map()
            .OnSharedStorageURNMappingResultDetermined(
                urn_uuid,
                CreateSharedStorageURNMappingResult(
                    storage_partition_, browser_context_, page_.get(),
                    main_frame_origin_, shared_storage_origin_,
                    shared_storage_site_, std::move(it->second),
                    /*index=*/0, /*budget_remaining=*/0.0, budget_status));

    shared_storage_worklet_host_manager_->NotifyConfigPopulated(config);

    it = unresolved_urns_.erase(it);
  }
}

void SharedStorageWorkletHost::SelectURL(
    const std::string& name,
    std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
        urls_with_metadata,
    blink::CloneableMessage serialized_data,
    bool keep_alive_after_operation,
    blink::mojom::PrivateAggregationConfigPtr private_aggregation_config,
    SelectURLCallback callback) {
  CHECK(private_aggregation_config);
  // `page_` can be null. See test
  // MainFrameDocumentAssociatedDataChangesOnSameSiteNavigation in
  // SitePerProcessBrowserTest.
  if (!page_) {
    std::move(callback).Run(
        /*success=*/false, /*error_message=*/
        "Internal error: page does not exist.",
        /*result_config=*/std::nullopt);
    return;
  }

  // Increment the page load metrics counter for selectURL calls.
  GetContentClient()->browser()->OnSharedStorageSelectURLCalled(
      &(page_->GetMainDocument()));

  // TODO(crbug.com/40946074): `document_service_` can somehow be null.
  if (!document_service_) {
    std::move(callback).Run(
        /*success=*/false, /*error_message=*/
        "Internal error: document does not exist.",
        /*result_config=*/std::nullopt);
    return;
  }

  if (!blink::IsValidSharedStorageURLsArrayLength(urls_with_metadata.size())) {
    // This could indicate a compromised renderer, so let's terminate it.
    receiver_.ReportBadMessage(
        "Attempted to execute RunURLSelectionOperationOnWorklet with invalid "
        "URLs array length.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kSelectURLNonWebVisibleInvalidURLArrayLength);
    return;
  }

  std::vector<SharedStorageEventParams::SharedStorageUrlSpecWithMetadata>
      converted_urls;
  for (const auto& url_with_metadata : urls_with_metadata) {
    // TODO(crbug.com/40223071): Use `blink::IsValidFencedFrameURL()` here.
    if (!url_with_metadata->url.is_valid()) {
      // This could indicate a compromised renderer, since the URLs were already
      // validated in the renderer.
      receiver_.ReportBadMessage(
          base::StrCat({"Invalid fenced frame URL '",
                        url_with_metadata->url.possibly_invalid_spec(), "'"}));
      LogSharedStorageWorkletError(
          blink::SharedStorageWorkletErrorType::
              kSelectURLNonWebVisibleInvalidFencedFrameURL);
      return;
    }

    std::map<std::string, std::string> reporting_metadata;
    for (const auto& metadata_pair : url_with_metadata->reporting_metadata) {
      if (!IsValidFencedFrameReportingURL(metadata_pair.second)) {
        // This could indicate a compromised renderer, since the reporting URLs
        // were already validated in the renderer.
        receiver_.ReportBadMessage(
            base::StrCat({"Invalid reporting URL '",
                          metadata_pair.second.possibly_invalid_spec(),
                          "' for '", metadata_pair.first, "'"}));
        LogSharedStorageWorkletError(
            blink::SharedStorageWorkletErrorType::
                kSelectURLNonWebVisibleInvalidReportingURL);
        return;
      }
      reporting_metadata.insert(
          std::make_pair(metadata_pair.first, metadata_pair.second.spec()));
    }

    converted_urls.emplace_back(url_with_metadata->url,
                                std::move(reporting_metadata));
  }

  if (private_aggregation_config->context_id.has_value()) {
    if (!blink::IsValidPrivateAggregationContextId(
            private_aggregation_config->context_id.value())) {
      receiver_.ReportBadMessage("Invalid context_id.");
      LogSharedStorageWorkletError(blink::SharedStorageWorkletErrorType::
                                       kSelectURLNonWebVisibleInvalidContextId);
      return;
    }

    if (document_service_->render_frame_host().IsNestedWithinFencedFrame() &&
        base::FeatureList::IsEnabled(
            blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
      receiver_.ReportBadMessage(
          "contextId cannot be set inside of fenced frames.");
      LogSharedStorageWorkletError(blink::SharedStorageWorkletErrorType::
                                       kSelectURLNonWebVisibleInvalidContextId);
      return;
    }
  }

  if (!blink::IsValidPrivateAggregationFilteringIdMaxBytes(
          private_aggregation_config->filtering_id_max_bytes)) {
    receiver_.ReportBadMessage("Invalid fitering_id_byte_size.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kSelectURLNonWebVisibleInvalidFilteringIdMaxBytes);
    return;
  }

  if (document_service_->render_frame_host().IsNestedWithinFencedFrame() &&
      private_aggregation_config->filtering_id_max_bytes !=
          blink::kPrivateAggregationApiDefaultFilteringIdMaxBytes &&
      base::FeatureList::IsEnabled(
          blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
    receiver_.ReportBadMessage(
        "filteringIdMaxBytes cannot be set inside of fenced frames.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kSelectURLNonWebVisibleInvalidFilteringIdMaxBytes);
    return;
  }

  if (!keep_alive_after_operation_) {
    receiver_.ReportBadMessage(
        "Received further operations when previous operation did not include "
        "the option \'keepAlive: true\'.");
    LogSharedStorageWorkletError(blink::SharedStorageWorkletErrorType::
                                     kSelectURLNonWebVisibleKeepAliveFalse);
    return;
  }

  // TODO(crbug.com/335818079): If `keep_alive_after_operation_` switches to
  // false, but the operation doesn't get executed (e.g. fails other checks), we
  // should destroy this worklet host as well.
  keep_alive_after_operation_ = keep_alive_after_operation;

  size_t shared_storage_fenced_frame_root_count = 0u;
  size_t fenced_frame_depth =
      static_cast<RenderFrameHostImpl&>(document_service_->render_frame_host())
          .frame_tree_node()
          ->GetFencedFrameDepth(shared_storage_fenced_frame_root_count);

  DCHECK_LE(shared_storage_fenced_frame_root_count, fenced_frame_depth);

  size_t max_allowed_fenced_frame_depth = base::checked_cast<size_t>(
      blink::features::kSharedStorageMaxAllowedFencedFrameDepthForSelectURL
          .Get());

  if (fenced_frame_depth > max_allowed_fenced_frame_depth) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/
        base::StrCat(
            {"selectURL() is called in a context with a fenced frame depth (",
             base::NumberToString(fenced_frame_depth),
             ") exceeding the maximum allowed number (",
             base::NumberToString(max_allowed_fenced_frame_depth), ")."}),
        /*result_config=*/std::nullopt);
    return;
  }

  auto pending_urn_uuid =
      page_->fenced_frame_urls_map().GeneratePendingMappedURN();

  if (!pending_urn_uuid.has_value()) {
    // Pending urn::uuid cannot be inserted to pending urn map because number of
    // urn mappings has reached limit.
    std::move(callback).Run(
        /*success=*/false, /*error_message=*/
        "sharedStorage.selectURL() failed because number of urn::uuid to url "
        "mappings has reached the limit.",
        /*result_config=*/std::nullopt);
    return;
  }

  GURL urn_uuid = pending_urn_uuid.value();

  std::string debug_message;
  bool prefs_failure_is_site_setting_specific = false;
  if (!IsSharedStorageSelectURLAllowed(
          &debug_message, &prefs_failure_is_site_setting_specific)) {
    if (is_same_origin_worklet_ || !prefs_failure_is_site_setting_specific) {
      std::move(callback).Run(
          /*success=*/false,
          /*error_message=*/
          GetSharedStorageErrorMessage(debug_message,
                                       kSharedStorageSelectURLDisabledMessage),
          /*result_config=*/std::nullopt);
    } else {
      // When the worklet and the worklet creator are not same-origin, the user
      // preferences for the worklet origin should not be revealed.
      LogSharedStorageWorkletError(
          blink::SharedStorageWorkletErrorType::
              kSelectURLNonWebVisibleCrossOriginSharedStorageDisabled);
      FencedFrameConfig config(urn_uuid, GURL());
      std::move(callback).Run(
          /*success=*/true, /*error_message=*/{},
          /*result_config=*/
          config.RedactFor(FencedFrameEntity::kEmbedder));
    }
    return;
  }

  // Further error checks/messages should be avoided, as they might indirectly
  // reveal the user preferences for the worklet origin.

  IncrementPendingOperationsCount();

  std::vector<GURL> urls;
  for (const auto& url_with_metadata : urls_with_metadata)
    urls.emplace_back(url_with_metadata->url);

  bool emplace_succeeded =
      unresolved_urns_.emplace(urn_uuid, std::move(urls_with_metadata)).second;

  // Assert that `urn_uuid` was not in the set before.
  DCHECK(emplace_succeeded);

  FencedFrameConfig config(urn_uuid, GURL());
  std::move(callback).Run(
      /*success=*/true, /*error_message=*/{},
      /*result_config=*/
      config.RedactFor(FencedFrameEntity::kEmbedder));

  shared_storage_worklet_host_manager_->NotifyUrnUuidGenerated(urn_uuid);

  shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
      AccessType::kDocumentSelectURL, document_service_->main_frame_id(),
      shared_storage_origin_.Serialize(),
      SharedStorageEventParams::CreateForSelectURL(name, serialized_data,
                                                   std::move(converted_urls)));

  GetAndConnectToSharedStorageWorkletService()->RunURLSelectionOperation(
      name, urls, std::move(serialized_data),
      MaybeConstructPrivateAggregationOperationDetails(
          private_aggregation_config),
      base::BindOnce(
          &SharedStorageWorkletHost::
              OnRunURLSelectionOperationOnWorkletScriptExecutionFinished,
          weak_ptr_factory_.GetWeakPtr(), urn_uuid, base::TimeTicks::Now()));
}

void SharedStorageWorkletHost::Run(
    const std::string& name,
    blink::CloneableMessage serialized_data,
    bool keep_alive_after_operation,
    blink::mojom::PrivateAggregationConfigPtr private_aggregation_config,
    RunCallback callback) {
  CHECK(private_aggregation_config);
  // `page_` can be null. See test
  // MainFrameDocumentAssociatedDataChangesOnSameSiteNavigation in
  // SitePerProcessBrowserTest.
  if (!page_) {
    std::move(callback).Run(
        /*success=*/false, /*error_message=*/
        "Internal error: page does not exist.");
    return;
  }

  // TODO(crbug.com/40946074): `document_service_` can somehow be null.
  if (!document_service_) {
    std::move(callback).Run(
        /*success=*/false, /*error_message=*/
        "Internal error: document does not exist.");
    return;
  }

  if (private_aggregation_config->context_id.has_value()) {
    if (!blink::IsValidPrivateAggregationContextId(
            private_aggregation_config->context_id.value())) {
      receiver_.ReportBadMessage("Invalid context_id.");
      LogSharedStorageWorkletError(blink::SharedStorageWorkletErrorType::
                                       kRunNonWebVisibleInvalidContextId);
      return;
    }

    if (document_service_->render_frame_host().IsNestedWithinFencedFrame() &&
        base::FeatureList::IsEnabled(
            blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
      receiver_.ReportBadMessage(
          "contextId cannot be set inside of fenced frames.");
      LogSharedStorageWorkletError(blink::SharedStorageWorkletErrorType::
                                       kRunNonWebVisibleInvalidContextId);
      return;
    }
  }

  if (!blink::IsValidPrivateAggregationFilteringIdMaxBytes(
          private_aggregation_config->filtering_id_max_bytes)) {
    receiver_.ReportBadMessage("Invalid fitering_id_byte_size.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kRunNonWebVisibleInvalidFilteringIdMaxBytes);
    return;
  }

  if (document_service_->render_frame_host().IsNestedWithinFencedFrame() &&
      private_aggregation_config->filtering_id_max_bytes !=
          blink::kPrivateAggregationApiDefaultFilteringIdMaxBytes &&
      base::FeatureList::IsEnabled(
          blink::features::kFencedFramesLocalUnpartitionedDataAccess)) {
    receiver_.ReportBadMessage(
        "filteringIdMaxBytes cannot be set inside of fenced frames.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kRunNonWebVisibleInvalidFilteringIdMaxBytes);
    return;
  }

  if (!keep_alive_after_operation_) {
    receiver_.ReportBadMessage(
        "Received further operations when previous operation did not include "
        "the option \'keepAlive: true\'.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kRunNonWebVisibleKeepAliveFalse);
    return;
  }

  // TODO(crbug.com/335818079): If `keep_alive_after_operation_` switches to
  // false, but the operation doesn't get executed (e.g. fails other checks), we
  // should destroy this worklet host as well.
  keep_alive_after_operation_ = keep_alive_after_operation;

  std::string debug_message;
  bool prefs_failure_is_site_setting_specific = false;
  if (!IsSharedStorageAllowed(&debug_message,
                              &prefs_failure_is_site_setting_specific)) {
    if (is_same_origin_worklet_ || !prefs_failure_is_site_setting_specific) {
      std::move(callback).Run(
          /*success=*/false,
          /*error_message=*/GetSharedStorageErrorMessage(
              debug_message, kSharedStorageDisabledMessage));
    } else {
      // When the worklet and the worklet creator are not same-origin, the user
      // preferences for the worklet origin should not be revealed.
      LogSharedStorageWorkletError(
          blink::SharedStorageWorkletErrorType::
              kRunNonWebVisibleCrossOriginSharedStorageDisabled);
      std::move(callback).Run(
          /*success=*/true,
          /*error_message=*/{});
    }
    return;
  }

  // Further error checks/messages should be avoided, as they might indirectly
  // reveal the user preferences for the worklet origin.

  IncrementPendingOperationsCount();

  std::move(callback).Run(/*success=*/true, /*error_message=*/{});

  shared_storage_worklet_host_manager_->NotifySharedStorageAccessed(
      AccessType::kDocumentRun, document_service_->main_frame_id(),
      shared_storage_origin_.Serialize(),
      SharedStorageEventParams::CreateForRun(name, serialized_data));

  GetAndConnectToSharedStorageWorkletService()->RunOperation(
      name, std::move(serialized_data),
      MaybeConstructPrivateAggregationOperationDetails(
          private_aggregation_config),
      base::BindOnce(&SharedStorageWorkletHost::OnRunOperationOnWorkletFinished,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
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
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/GetSharedStorageErrorMessage(
            debug_message, kSharedStorageDisabledMessage));
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
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/GetSharedStorageErrorMessage(
            debug_message, kSharedStorageDisabledMessage));
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
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/GetSharedStorageErrorMessage(
            debug_message, kSharedStorageDisabledMessage));
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
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/GetSharedStorageErrorMessage(
            debug_message, kSharedStorageDisabledMessage));
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
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(blink::mojom::SharedStorageGetStatus::kError,
                            /*error_message=*/
                            GetSharedStorageErrorMessage(
                                debug_message, kSharedStorageDisabledMessage),
                            /*value=*/{});
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
              blink::mojom::SharedStorageGetStatus::kNotFound,
              /*error_message=*/"sharedStorage.get() could not find key",
              /*value=*/{});
          return;
        }

        if (result.result != OperationResult::kSuccess) {
          std::move(callback).Run(
              blink::mojom::SharedStorageGetStatus::kError,
              /*error_message=*/"sharedStorage.get() failed", /*value=*/{});
          return;
        }

        std::move(callback).Run(blink::mojom::SharedStorageGetStatus::kSuccess,
                                /*error_message=*/{}, /*value=*/result.data);
      },
      std::move(callback));

  shared_storage_manager_->Get(shared_storage_origin_, key,
                               std::move(operation_completed_callback));
}

void SharedStorageWorkletHost::SharedStorageKeys(
    mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
        pending_listener) {
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    mojo::Remote<blink::mojom::SharedStorageEntriesListener> listener(
        std::move(pending_listener));
    listener->DidReadEntries(
        /*success=*/false,
        GetSharedStorageErrorMessage(debug_message,
                                     kSharedStorageDisabledMessage),
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
    mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
        pending_listener) {
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    mojo::Remote<blink::mojom::SharedStorageEntriesListener> listener(
        std::move(pending_listener));
    listener->DidReadEntries(
        /*success=*/false,
        GetSharedStorageErrorMessage(debug_message,
                                     kSharedStorageDisabledMessage),
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
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/
        GetSharedStorageErrorMessage(debug_message,
                                     kSharedStorageDisabledMessage),
        /*length=*/0);
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
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(
        /*success=*/false,
        /*error_message=*/
        GetSharedStorageErrorMessage(debug_message,
                                     kSharedStorageDisabledMessage),
        /*bits=*/0.0);
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
      shared_storage_site_, std::move(operation_completed_callback));
}

void SharedStorageWorkletHost::DidAddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  if (!document_service_) {
    DCHECK(IsInKeepAlivePhase());
    return;
  }

  // Mimic what's being done for console outputs from Window context, which
  // manually triggers the observer method.
  static_cast<RenderFrameHostImpl&>(document_service_->render_frame_host())
      .DidAddMessageToConsole(level, base::UTF8ToUTF16(message),
                              /*line_no=*/0, /*source_id=*/{},
                              /*untrusted_stack_trace=*/{});
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

void SharedStorageWorkletHost::ReportNoBinderForInterface(
    const std::string& error) {
  broker_receiver_.ReportBadMessage(error +
                                    " for the shared storage worklet scope");
}

RenderProcessHost* SharedStorageWorkletHost::GetProcessHost() const {
  return driver_->GetProcessHost();
}

RenderFrameHostImpl* SharedStorageWorkletHost::GetFrame() {
  if (document_service_) {
    return static_cast<RenderFrameHostImpl*>(
        &document_service_->render_frame_host());
  }

  return nullptr;
}

void SharedStorageWorkletHost::OnAddModuleOnWorkletFinished(
    blink::mojom::SharedStorageDocumentService::CreateWorkletCallback callback,
    bool success,
    const std::string& error_message) {
  // After the initial script loading, accessing shared storage will be allowed.
  // We want to disable the communication with network and with the cache, to
  // prevent leaking shared storage data.
  //
  // Note: The last code cache message (i.e. `DidGenerateCacheableMetadata()`,
  // if any) could race with this `OnAddModuleOnWorkletFinished()` callback, as
  // they are from separate mojom channels. It could impact the utility (i.e.
  // the generated code is not stored when the race happens).
  //
  // TODO(crbug.com/341690728): Measure how often the race happens, and
  // rearchitect if necessary.
  url_loader_factory_proxy_.reset();
  code_cache_host_proxy_.reset();

  std::move(callback).Run(success, error_message);

  DecrementPendingOperationsCount();
}

void SharedStorageWorkletHost::OnRunOperationOnWorkletFinished(
    base::TimeTicks start_time,
    bool success,
    const std::string& error_message) {
  if (!success) {
    LogSharedStorageWorkletErrorFromErrorMessage(/*from_select_url=*/false,
                                                 error_message);
    if (document_service_) {
      DCHECK(!IsInKeepAlivePhase());
      devtools_instrumentation::LogWorkletMessage(
          static_cast<RenderFrameHostImpl&>(
              document_service_->render_frame_host()),
          blink::mojom::ConsoleMessageLevel::kError, error_message);
    }
  } else {
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kSuccess);
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
  CHECK(it != unresolved_urns_.end(), base::NotFatalUntil::M130);

  if ((success && index >= it->second.size()) || (!success && index != 0)) {
    // This could indicate a compromised worklet environment, so let's terminate
    // it.
    shared_storage_worklet_service_client_.ReportBadMessage(
        "Unexpected index number returned from selectURL().");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kSelectURLNonWebVisibleUnexpectedIndexReturned);

    unresolved_urns_.erase(it);
    DecrementPendingOperationsCount();
    return;
  }

  shared_storage_manager_->GetRemainingBudget(
      shared_storage_site_,
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
  CHECK(it != unresolved_urns_.end(), base::NotFatalUntil::M130);

  std::vector<blink::mojom::SharedStorageUrlWithMetadataPtr>
      urls_with_metadata = std::move(it->second);
  unresolved_urns_.erase(it);

  if (page_) {
    blink::SharedStorageSelectUrlBudgetStatus budget_status =
        blink::SharedStorageSelectUrlBudgetStatus::kOther;

    SharedStorageURNMappingResult mapping_result =
        CreateSharedStorageURNMappingResult(
            storage_partition_, browser_context_, page_.get(),
            main_frame_origin_, shared_storage_origin_, shared_storage_site_,
            std::move(urls_with_metadata), index, budget_result.bits,
            budget_status);

    // Log histograms. These do not need the `document_service_`.
    blink::LogSharedStorageSelectURLBudgetStatus(budget_status);
    if (budget_status !=
        blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget) {
      LogSharedStorageWorkletError(
          blink::SharedStorageWorkletErrorType::
              kSelectURLNonWebVisibleInsufficientBudget);
    } else if (!script_execution_succeeded) {
      LogSharedStorageWorkletErrorFromErrorMessage(
          /*from_select_url=*/true, script_execution_error_message);
    } else {
      LogSharedStorageWorkletError(
          blink::SharedStorageWorkletErrorType::kSuccess);
    }

    if (document_service_) {
      DCHECK(!IsInKeepAlivePhase());

      // Let the insufficient-budget failure supersede the script failure.
      if (budget_status !=
          blink::SharedStorageSelectUrlBudgetStatus::kSufficientBudget) {
        devtools_instrumentation::LogWorkletMessage(
            static_cast<RenderFrameHostImpl&>(
                document_service_->render_frame_host()),
            blink::mojom::ConsoleMessageLevel::kError,
            "Insufficient budget for selectURL().");
      } else if (!script_execution_succeeded) {
        devtools_instrumentation::LogWorkletMessage(
            static_cast<RenderFrameHostImpl&>(
                document_service_->render_frame_host()),
            blink::mojom::ConsoleMessageLevel::kError,
            script_execution_error_message);
      }
    }

    std::optional<FencedFrameConfig> config =
        page_->fenced_frame_urls_map()
            .OnSharedStorageURNMappingResultDetermined(
                urn_uuid, std::move(mapping_result));

    shared_storage_worklet_host_manager_->NotifyConfigPopulated(config);
  } else {
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kSelectURLWebVisible);
  }

  base::UmaHistogramLongTimes(
      "Storage.SharedStorage.Document.Timing.SelectURL.ExecutedInWorklet",
      base::TimeTicks::Now() - start_time);
  DecrementPendingOperationsCount();
}

void SharedStorageWorkletHost::ExpireWorklet() {
  // `this` is not in keep-alive.
  DCHECK(document_service_);
  DCHECK(shared_storage_worklet_host_manager_);

  // This will remove this worklet host from the manager.
  shared_storage_worklet_host_manager_->ExpireWorkletHostForDocumentService(
      document_service_.get(), this);

  // Do not add code after this. SharedStorageWorkletHost has been destroyed.
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

  if (!IsInKeepAlivePhase() && keep_alive_after_operation_) {
    return;
  }

  if (IsInKeepAlivePhase()) {
    FinishKeepAlive(/*timeout_reached=*/false);
    return;
  }

  ExpireWorklet();

  // Do not add code after here. The worklet will be closed.
}

base::TimeDelta SharedStorageWorkletHost::GetKeepAliveTimeout() const {
  return kKeepAliveTimeout;
}

blink::mojom::SharedStorageWorkletService*
SharedStorageWorkletHost::GetAndConnectToSharedStorageWorkletService() {
  DCHECK(document_service_);

  if (!shared_storage_worklet_service_) {
    code_cache_host_receivers_ =
        std::make_unique<CodeCacheHostImpl::ReceiverSet>(
            storage_partition_->GetGeneratedCodeCacheContext());

    RenderFrameHostImpl& rfh = static_cast<RenderFrameHostImpl&>(
        document_service_->render_frame_host());

    mojo::PendingRemote<blink::mojom::CodeCacheHost> actual_code_cache_host;
    code_cache_host_receivers_->Add(
        rfh.GetProcess()->GetID(), rfh.GetNetworkIsolationKey(),
        rfh.GetStorageKey(),
        actual_code_cache_host.InitWithNewPipeAndPassReceiver());

    mojo::PendingRemote<blink::mojom::CodeCacheHost> proxied_code_cache_host;
    code_cache_host_proxy_ = std::make_unique<SharedStorageCodeCacheHostProxy>(
        std::move(actual_code_cache_host),
        proxied_code_cache_host.InitWithNewPipeAndPassReceiver(),
        script_source_url_);

    mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
        browser_interface_broker;
    broker_receiver_.Bind(
        browser_interface_broker.InitWithNewPipeAndPassReceiver());

    auto global_scope_creation_params =
        blink::mojom::WorkletGlobalScopeCreationParams::New(
            script_source_url_, shared_storage_origin_, origin_trial_features_,
            devtools_handle_->devtools_token(),
            devtools_handle_->BindNewPipeAndPassRemote(),
            std::move(proxied_code_cache_host),
            std::move(browser_interface_broker),
            devtools_handle_->wait_for_debugger());

    driver_->StartWorkletService(
        shared_storage_worklet_service_.BindNewPipeAndPassReceiver(),
        std::move(global_scope_creation_params));

    const blink::PermissionsPolicy* permissions_policy =
        rfh.permissions_policy();

    bool private_aggregation_permissions_policy_allowed =
        permissions_policy->IsFeatureEnabledForOrigin(
            blink::mojom::PermissionsPolicyFeature::kPrivateAggregation,
            shared_storage_origin_);

    auto embedder_context = static_cast<RenderFrameHostImpl&>(
                                document_service_->render_frame_host())
                                .frame_tree_node()
                                ->GetEmbedderSharedStorageContextIfAllowed();

    shared_storage_worklet_service_->Initialize(
        shared_storage_worklet_service_client_.BindNewEndpointAndPassRemote(),
        private_aggregation_permissions_policy_allowed, embedder_context);
  }

  return shared_storage_worklet_service_.get();
}

blink::mojom::PrivateAggregationOperationDetailsPtr
SharedStorageWorkletHost::MaybeConstructPrivateAggregationOperationDetails(
    const blink::mojom::PrivateAggregationConfigPtr&
        private_aggregation_config) {
  CHECK(browser_context_);
  CHECK(private_aggregation_config);

  if (!blink::ShouldDefinePrivateAggregationInSharedStorage()) {
    return nullptr;
  }

  PrivateAggregationManager* private_aggregation_manager =
      PrivateAggregationManager::GetManager(*browser_context_);
  CHECK(private_aggregation_manager);

  blink::mojom::PrivateAggregationOperationDetailsPtr pa_operation_details =
      blink::mojom::PrivateAggregationOperationDetails::New(
          mojo::PendingRemote<blink::mojom::PrivateAggregationHost>(),
          private_aggregation_config->filtering_id_max_bytes);

  std::optional<base::TimeDelta> timeout =
      (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPIM118) &&
       private_aggregation_config->context_id)
          ? std::optional<base::TimeDelta>(base::Seconds(5))
          : std::nullopt;

  // TODO(crbug.com/330744610): Allow filtering ID byte size to be set.
  bool success = private_aggregation_manager->BindNewReceiver(
      shared_storage_origin_, main_frame_origin_,
      PrivateAggregationCallerApi::kSharedStorage,
      private_aggregation_config->context_id, std::move(timeout),
      private_aggregation_config->aggregation_coordinator_origin,
      private_aggregation_config->filtering_id_max_bytes,
      pa_operation_details->pa_host.InitWithNewPipeAndPassReceiver());
  CHECK(success);

  return pa_operation_details;
}

bool SharedStorageWorkletHost::IsSharedStorageAllowed(
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  RenderFrameHost* rfh =
      document_service_ ? &(document_service_->render_frame_host()) : nullptr;
  return GetContentClient()->browser()->IsSharedStorageAllowed(
      browser_context_, rfh, main_frame_origin_, shared_storage_origin_,
      out_debug_message, out_block_is_site_setting_specific);
}

bool SharedStorageWorkletHost::IsSharedStorageSelectURLAllowed(
    std::string* out_debug_message,
    bool* out_block_is_site_setting_specific) {
  CHECK(document_service_);

  // Will trigger a call to
  // `content_settings::PageSpecificContentSettings::BrowsingDataAccessed()` for
  // reporting purposes.
  if (!IsSharedStorageAllowed(out_debug_message,
                              out_block_is_site_setting_specific)) {
    return false;
  }

  return GetContentClient()->browser()->IsSharedStorageSelectURLAllowed(
      browser_context_, main_frame_origin_, shared_storage_origin_,
      out_debug_message, out_block_is_site_setting_specific);
}

}  // namespace content
