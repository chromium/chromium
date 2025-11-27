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
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/unguessable_token.h"
#include "components/metrics/dwa/dwa_builders.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/shared_storage_worklet_devtools_manager.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/private_aggregation/private_aggregation_caller_api.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/shared_storage/shared_storage_code_cache_host_proxy.h"
#include "content/browser/shared_storage/shared_storage_document_service_impl.h"
#include "content/browser/shared_storage/shared_storage_lock_manager.h"
#include "content/browser/shared_storage/shared_storage_render_thread_worklet_driver.h"
#include "content/browser/shared_storage/shared_storage_runtime_manager.h"
#include "content/browser/shared_storage/shared_storage_url_loader_factory_proxy.h"
#include "content/browser/shared_storage/shared_storage_worklet_driver.h"
#include "content/common/renderer.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/shared_storage.mojom.h"
#include "storage/browser/blob/blob_url_loader_factory.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/shared_storage/shared_storage_utils.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

using AccessScope = blink::SharedStorageAccessScope;
using AccessMethod =
    SharedStorageRuntimeManager::SharedStorageObserverInterface::AccessMethod;

constexpr base::TimeDelta kKeepAliveTimeout = base::Seconds(2);

constexpr base::TimeDelta kOptInRequestTimeout = base::Seconds(30);

constexpr int kMaxOptInResponseBodySize = 8192;

constexpr char kAsteriskWildcard[] = "*";

using SharedStorageURNMappingResult =
    FencedFrameURLMapping::SharedStorageURNMappingResult;

using OperationResult = storage::SharedStorageManager::OperationResult;
using GetResult = storage::SharedStorageManager::GetResult;

net::NetworkTrafficAnnotationTag CreateDataOptInNetworkAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("shared_storage_worklet_host", R"(
    semantics {
      sender: "Shared Storage Worklet Host"
      description:
        "Requests the /.well-known/shared-storage/trusted-origins for the data "
        "origin to be used by worklet in the Shared Storage API, if the data "
        "origin is cross-origin to both the context origin of the worklet and "
        "the URL of the worklet script, to check whether the data origin has "
        "opted-into processing by cross-origin worklets; see "
        "https://github.com/WICG/shared-storage."
      trigger:
        "When `sharedStorage.createWorklet()` is called with a `dataOrigin` "
        "that is cross-origin to both the worklet context and script origins."
      data:
        "Request headers only."
      destination: WEBSITE
      internal {
        contacts {
          email: "privacy-sandbox-dev@chromium.org"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2024-11-15"
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature can be controlled via the 'Site-suggested ads' and "
        "'Ad measurement' settings in the 'Ad privacy' section of 'Privacy "
        "and Security'."
      chrome_policy {
        PrivacySandboxAdMeasurementEnabled {
          PrivacySandboxAdMeasurementEnabled: false
        },
        PrivacySandboxSiteEnabledAdsEnabled {
          PrivacySandboxSiteEnabledAdsEnabled: false
        }
      }
    })");
}

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
    bool use_page_budgets,
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
    budget_status =
        (budget_to_charge > budget_remaining)
            ? blink::SharedStorageSelectUrlBudgetStatus::
                  kInsufficientSiteNavigationBudget
            : (use_page_budgets ? page->CheckAndMaybeDebitSelectURLBudgets(
                                      shared_storage_site, budget_to_charge)
                                : blink::SharedStorageSelectUrlBudgetStatus::
                                      kSufficientBudget);
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

blink::mojom::SharedStorageWorkletPermissionsPolicyStatePtr
GetSharedStorageWorkletPermissionsPolicyState(
    RenderFrameHostImpl& creator_document,
    const url::Origin& shared_storage_origin) {
  const network::PermissionsPolicy* permissions_policy =
      creator_document.GetPermissionsPolicy();

  return blink::mojom::SharedStorageWorkletPermissionsPolicyState::New(
      permissions_policy->IsFeatureEnabledForOrigin(
          network::mojom::PermissionsPolicyFeature::kPrivateAggregation,
          shared_storage_origin),
      permissions_policy->IsFeatureEnabledForOrigin(
          network::mojom::PermissionsPolicyFeature::kJoinAdInterestGroup,
          shared_storage_origin),
      permissions_policy->IsFeatureEnabledForOrigin(
          network::mojom::PermissionsPolicyFeature::kRunAdAuction,
          shared_storage_origin));
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
    blink::mojom::SharedStorageDataOriginType data_origin_type,
    const GURL& script_source_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::SharedStorageWorkletCreationMethod creation_method,
    int worklet_ordinal,
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
      shared_storage_runtime_manager_(
          storage_partition_->GetSharedStorageRuntimeManager()),
      browser_context_(
          document_service.render_frame_host().GetBrowserContext()),
      creation_method_(creation_method),
      shared_storage_origin_(data_origin),
      shared_storage_site_(net::SchemefulSite(shared_storage_origin_)),
      main_frame_origin_(document_service.main_frame_origin()),
      creator_context_origin_(
          document_service.render_frame_host().GetLastCommittedOrigin()),
      is_same_origin_worklet_(
          creator_context_origin_.IsSameOriginWith(shared_storage_origin_)),
      needs_data_origin_opt_in_(
          !is_same_origin_worklet_ &&
          !data_origin.IsSameOriginWith(script_source_url)),
      saved_queries_enabled_(base::FeatureList::IsEnabled(
          blink::features::kSharedStorageSelectURLSavedQueries)),
      source_id_(page_->GetMainDocument().GetPageUkmSourceId()),
      worklet_ordinal_(worklet_ordinal),
      creation_time_(base::TimeTicks::Now()) {
  GetContentClient()->browser()->OnSharedStorageWorkletHostCreated(
      &(document_service.render_frame_host()));

  receiver_.Bind(std::move(worklet_host));

  if (!base::FeatureList::IsEnabled(
          blink::features::kSharedStorageCreateWorkletCustomDataOrigin) &&
      needs_data_origin_opt_in_) {
    // This could indicate a compromised renderer, so let's terminate it.
    receiver_.ReportBadMessage(
        "Attempted to use a custom origin when disabled.");
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::
            kAddModuleNonWebVisibleCustomDataOriginDisabled);
    return;
  }

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

  // The render frame host must have a permissions policy.
  CHECK(
      static_cast<RenderFrameHostImpl&>(document_service_->render_frame_host())
          .GetPermissionsPolicy());

  url_loader_factory_proxy_ =
      std::make_unique<SharedStorageURLLoaderFactoryProxy>(
          std::move(frame_url_loader_factory),
          url_loader_factory.InitWithNewPipeAndPassReceiver(), frame_origin,
          shared_storage_origin_, script_source_url, credentials_mode,
          static_cast<RenderFrameHostImpl&>(
              document_service_->render_frame_host())
              .ComputeSiteForCookies(),
          *static_cast<RenderFrameHostImpl&>(
               document_service_->render_frame_host())
               .GetPermissionsPolicy());

  if (creation_method ==
      blink::mojom::SharedStorageWorkletCreationMethod::kAddModule) {
    shared_storage_runtime_manager_->NotifySharedStorageAccessed(
        AccessScope::kWindow, AccessMethod::kAddModule,
        document_service_->main_frame_id(), shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateForAddModule(
            script_source_url, worklet_ordinal_, GetWorkletDevToolsToken()));
  } else {
    std::string data_origin_type_string =
        (data_origin_type ==
         blink::mojom::SharedStorageDataOriginType::kContextOrigin)
            ? "context-origin"
            : ((data_origin_type ==
                blink::mojom::SharedStorageDataOriginType::kScriptOrigin)
                   ? "script-origin"
                   : data_origin.Serialize());

    shared_storage_runtime_manager_->NotifySharedStorageAccessed(
        AccessScope::kWindow, AccessMethod::kCreateWorklet,
        document_service_->main_frame_id(), shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateForCreateWorklet(
            script_source_url, data_origin_type_string, worklet_ordinal_,
            GetWorkletDevToolsToken()));
  }

  create_worklet_finished_callback_ = std::move(callback);

  GetAndConnectToSharedStorageWorkletService()->AddModule(
      std::move(url_loader_factory), script_source_url,
      base::BindOnce(
          &SharedStorageWorkletHost::OnCreateWorkletScriptLoadingFinished,
          weak_ptr_factory_.GetWeakPtr()));

  if (!needs_data_origin_opt_in_) {
    return;
  }

  document_service_->render_frame_host().CreateNetworkServiceDefaultFactory(
      data_origin_opt_in_url_loader_factory_.BindNewPipeAndPassReceiver());

  // Construct resource request for .well-known URL.
  auto data_origin_opt_in_request =
      std::make_unique<network::ResourceRequest>();
  GURL url = shared_storage_origin_.GetURL();
  GURL::Replacements replacements;
  replacements.SetPathStr("/.well-known/shared-storage/trusted-origins");
  data_origin_opt_in_request->url = url.ReplaceComponents(replacements);
  data_origin_opt_in_request->method = net::HttpRequestHeaders::kGetMethod;
  data_origin_opt_in_request->headers.SetHeader(
      net::HttpRequestHeaders::kAccept, "application/json");
  data_origin_opt_in_request->headers.SetHeader(
      net::HttpRequestHeaders::kOrigin, creator_context_origin_.Serialize());
  data_origin_opt_in_request->credentials_mode =
      network::mojom::CredentialsMode::kOmit;

  // These requests are JSON requests made using a URLLoaderFactory matching
  // the one created for the renderer process. Therefore, CORS needs to be
  // enabled to avoid ORB blocking.
  data_origin_opt_in_request->mode = network::mojom::RequestMode::kCors;
  data_origin_opt_in_request->request_initiator = creator_context_origin_;
  data_origin_opt_in_request->redirect_mode =
      network::mojom::RedirectMode::kFollow;
  data_origin_opt_in_request->destination =
      network::mojom::RequestDestination::kJson;

  data_origin_opt_in_url_loader_ =
      network::SimpleURLLoader::Create(std::move(data_origin_opt_in_request),
                                       CreateDataOptInNetworkAnnotationTag());
  data_origin_opt_in_url_loader_->SetTimeoutDuration(kOptInRequestTimeout);
  data_origin_opt_in_url_loader_->SetRequestID(
      GlobalRequestID::MakeBrowserInitiated().request_id);
  data_origin_opt_in_url_loader_->DownloadToString(
      data_origin_opt_in_url_loader_factory_.get(),
      base::BindOnce(&SharedStorageWorkletHost::OnOptInRequestComplete,
                     base::Unretained(this)),
      kMaxOptInResponseBodySize);
}

SharedStorageWorkletHost::~SharedStorageWorkletHost() {
  base::UmaHistogramEnumeration("Storage.SharedStorage.Worklet.DestroyedStatus",
                                destroyed_status_);

  base::UmaHistogramBoolean(
      "Storage.SharedStorage.Worklet.NavigatorLocksInvoked",
      navigator_locks_invoked_);

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

  // Initialize to zero. This represents the scenario where the worklet didn't
  // execute any operations due to early validation failure, ensuring
  // consistency with the scope of the `UsefulResourceDuration` metric.
  base::TimeDelta useful_resource_duration;
  if (pending_operations_count_ > 0) {
    // Worklet is still processing, so the useful duration is its total
    // lifetime.
    useful_resource_duration = elapsed_time_since_creation;
  } else if (!last_operation_finished_time_.is_null()) {
    // Worklet finished at least one operation. Useful duration is until the
    // last operation.
    useful_resource_duration = last_operation_finished_time_ - creation_time_;
  }

  base::UmaHistogramTimes(
      "Storage.SharedStorage.Worklet.Timing.AbsoluteUsefulResourceDuration",
      useful_resource_duration);

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::SharedStorage_Worklet_OnDestroyed builder(source_id_);

  builder.SetAbsoluteUsefulResourceDuration(
      ukm::GetExponentialBucketMinForUserTiming(
          useful_resource_duration.InMilliseconds()));

  builder.Record(ukm_recorder->Get());

  if (!page_) {
    return;
  }

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
                urn_uuid, CreateSharedStorageURNMappingResult(
                              storage_partition_, browser_context_, page_.get(),
                              main_frame_origin_, shared_storage_origin_,
                              shared_storage_site_, std::move(it->second),
                              /*index=*/0, /*use_page_budgets=*/false,
                              /*budget_remaining=*/0.0, budget_status));

    shared_storage_runtime_manager_->NotifyConfigPopulated(config);

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
    bool resolve_to_config,
    const std::u16string& saved_query_name,
    base::TimeTicks start_time,
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

  size_t max_allowed_fenced_frame_depth =
      network::features::kSharedStorageMaxAllowedFencedFrameDepthForSelectURL
          .Get();

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
  for (const auto& url_with_metadata : urls_with_metadata) {
    urls.emplace_back(url_with_metadata->url);
  }

  bool emplace_succeeded =
      unresolved_urns_.emplace(urn_uuid, std::move(urls_with_metadata)).second;

  // Assert that `urn_uuid` was not in the set before.
  DCHECK(emplace_succeeded);

  FencedFrameConfig config(urn_uuid, GURL());
  std::move(callback).Run(
      /*success=*/true, /*error_message=*/{},
      /*result_config=*/
      config.RedactFor(FencedFrameEntity::kEmbedder));

  shared_storage_runtime_manager_->NotifyUrnUuidGenerated(urn_uuid);

  int operation_id = next_operation_id_++;

  shared_storage_runtime_manager_->NotifySharedStorageAccessed(
      AccessScope::kWindow, AccessMethod::kSelectURL,
      document_service_->main_frame_id(), shared_storage_origin_.Serialize(),
      SharedStorageEventParams::CreateForSelectURL(
          name, operation_id, keep_alive_after_operation,
          private_aggregation_config, serialized_data,
          std::move(converted_urls), resolve_to_config,
          base::UTF16ToUTF8(saved_query_name), urn_uuid,
          GetWorkletDevToolsToken()));

  if (saved_queries_enabled_ && !saved_query_name.empty()) {
    auto saved_query_callback =
        base::BindOnce(&SharedStorageWorkletHost::OnSelectURLSavedQueryFound,
                       weak_ptr_factory_.GetWeakPtr(), urn_uuid, start_time,
                       base::TimeTicks::Now(), operation_id, name);
    int32_t index = page_->GetSavedQueryResultIndexOrStoreCallback(
        shared_storage_origin_, script_source_url_, name, saved_query_name,
        std::move(saved_query_callback));
    if (index >= 0) {
      // The result index has been stored from a previously resolved worklet
      // operation.
      OnSelectURLSavedQueryFound(urn_uuid, start_time, base::TimeTicks::Now(),
                                 operation_id, name, index);
      return;
    }
    if (index == -1) {
      // The result index will be determined when a previously initiated worklet
      // operation finishes running. A callback (`saved_query_callback`) will
      // notify us of the result.
      return;
    }

    // The result index will be determined by running the registered worklet
    // operation via
    // `blink::mojom::SharedStorageWorkletService::RunURLSelectionOperation()`.
    CHECK_EQ(index, -2);
  }

  GetAndConnectToSharedStorageWorkletService()->RunURLSelectionOperation(
      name, urls, std::move(serialized_data),
      MaybeConstructPrivateAggregationOperationDetails(
          private_aggregation_config),
      base::BindOnce(
          &SharedStorageWorkletHost::
              OnRunURLSelectionOperationOnWorkletScriptExecutionFinished,
          weak_ptr_factory_.GetWeakPtr(), urn_uuid, start_time,
          base::TimeTicks::Now(), operation_id, name, saved_query_name));
}

void SharedStorageWorkletHost::Run(
    const std::string& name,
    blink::CloneableMessage serialized_data,
    bool keep_alive_after_operation,
    blink::mojom::PrivateAggregationConfigPtr private_aggregation_config,
    base::TimeTicks start_time,
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

  int operation_id = next_operation_id_++;

  shared_storage_runtime_manager_->NotifySharedStorageAccessed(
      AccessScope::kWindow, AccessMethod::kRun,
      document_service_->main_frame_id(), shared_storage_origin_.Serialize(),
      SharedStorageEventParams::CreateForRun(
          name, operation_id, keep_alive_after_operation,
          private_aggregation_config, serialized_data,
          GetWorkletDevToolsToken()));

  GetAndConnectToSharedStorageWorkletService()->RunOperation(
      name, std::move(serialized_data),
      MaybeConstructPrivateAggregationOperationDetails(
          private_aggregation_config),
      base::BindOnce(&SharedStorageWorkletHost::OnRunOperationOnWorkletFinished,
                     weak_ptr_factory_.GetWeakPtr(), start_time,
                     base::TimeTicks::Now(), operation_id));
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

void SharedStorageWorkletHost::SharedStorageUpdate(
    network::mojom::SharedStorageModifierMethodWithOptionsPtr
        method_with_options,
    SharedStorageUpdateCallback callback) {
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(GetSharedStorageErrorMessage(
        debug_message, kSharedStorageDisabledMessage));
    return;
  }

  shared_storage_runtime_manager_->lock_manager().SharedStorageUpdate(
      std::move(method_with_options), shared_storage_origin_,
      AccessScope::kSharedStorageWorklet, GetMainFrameIdIfAvailable(),
      GetWorkletDevToolsToken(), std::move(callback));
}

void SharedStorageWorkletHost::SharedStorageBatchUpdate(
    std::vector<network::mojom::SharedStorageModifierMethodWithOptionsPtr>
        methods_with_options,
    const std::optional<std::string>& with_lock,
    SharedStorageBatchUpdateCallback callback) {
  std::string debug_message;
  if (!IsSharedStorageAllowed(&debug_message)) {
    std::move(callback).Run(GetSharedStorageErrorMessage(
        debug_message, kSharedStorageDisabledMessage));
    return;
  }

  shared_storage_runtime_manager_->lock_manager().SharedStorageBatchUpdate(
      std::move(methods_with_options), with_lock, shared_storage_origin_,
      AccessScope::kSharedStorageWorklet, GetMainFrameIdIfAvailable(),
      GetWorkletDevToolsToken(), std::move(callback));
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
    shared_storage_runtime_manager_->NotifySharedStorageAccessed(
        AccessScope::kSharedStorageWorklet, AccessMethod::kGet,
        document_service_->main_frame_id(), shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateForGet(base::UTF16ToUTF8(key),
                                               GetWorkletDevToolsToken()));
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
    shared_storage_runtime_manager_->NotifySharedStorageAccessed(
        AccessScope::kSharedStorageWorklet, AccessMethod::kKeys,
        document_service_->main_frame_id(), shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateWithWorkletToken(
            GetWorkletDevToolsToken()));
  }

  shared_storage_manager_->Keys(shared_storage_origin_,
                                std::move(pending_listener), base::DoNothing());
}

void SharedStorageWorkletHost::SharedStorageEntries(
    mojo::PendingRemote<blink::mojom::SharedStorageEntriesListener>
        pending_listener,
    bool values_only) {
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
    shared_storage_runtime_manager_->NotifySharedStorageAccessed(
        AccessScope::kSharedStorageWorklet,
        values_only ? AccessMethod::kValues : AccessMethod::kEntries,
        document_service_->main_frame_id(), shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateWithWorkletToken(
            GetWorkletDevToolsToken()));
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
    shared_storage_runtime_manager_->NotifySharedStorageAccessed(
        AccessScope::kSharedStorageWorklet, AccessMethod::kLength,
        document_service_->main_frame_id(), shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateWithWorkletToken(
            GetWorkletDevToolsToken()));
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
    shared_storage_runtime_manager_->NotifySharedStorageAccessed(
        AccessScope::kSharedStorageWorklet, AccessMethod::kRemainingBudget,
        document_service_->main_frame_id(), shared_storage_origin_.Serialize(),
        SharedStorageEventParams::CreateWithWorkletToken(
            GetWorkletDevToolsToken()));
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

void SharedStorageWorkletHost::GetInterestGroups(
    GetInterestGroupsCallback callback) {
  InterestGroupManagerImpl* interest_group_manager =
      static_cast<InterestGroupManagerImpl*>(
          storage_partition_->GetInterestGroupManager());

  if (!interest_group_manager) {
    std::move(callback).Run(
        blink::mojom::GetInterestGroupsResult::NewErrorMessage(
            "InterestGroupManager unavailable."));
    return;
  }

  RenderFrameHost* rfh =
      document_service_ ? &document_service_->render_frame_host() : nullptr;

  if (!GetContentClient()->browser()->IsInterestGroupAPIAllowed(
          browser_context_, rfh, InterestGroupApiOperation::kRead,
          main_frame_origin_, shared_storage_origin_)) {
    std::move(callback).Run(
        blink::mojom::GetInterestGroupsResult::NewErrorMessage(
            "interestGroups() is not allowed."));
    return;
  }

  interest_group_manager->GetInterestGroupsForOwner(
      /*devtools_auction_id=*/{},
      /*owner=*/shared_storage_origin_,
      base::BindOnce(
          [](base::ElapsedTimer timer, GetInterestGroupsCallback callback,
             scoped_refptr<StorageInterestGroups> groups) {
            std::vector<blink::mojom::StorageInterestGroupPtr> mojom_groups;

            for (SingleStorageInterestGroup& group :
                 groups->GetInterestGroups()) {
              blink::mojom::StorageInterestGroupPtr mojom_group =
                  blink::mojom::StorageInterestGroup::New(
                      group->interest_group,
                      group->bidding_browser_signals->Clone(),
                      group->joining_origin, group->join_time,
                      group->last_updated, group->next_update_after,
                      group->interest_group.EstimateSize());

              mojom_groups.push_back(std::move(mojom_group));
            }

            base::UmaHistogramTimes(
                "Storage.SharedStorage.InterestGroups.InBrowserRetrievalTime",
                timer.Elapsed());

            std::move(callback).Run(
                blink::mojom::GetInterestGroupsResult::NewGroups(
                    std::move(mojom_groups)));
          },
          base::ElapsedTimer(), std::move(callback)));
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
  if (!page_) {
    return;
  }

  for (blink::mojom::WebFeature feature : features) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        &page_->GetMainDocument(), feature);
  }
}

void SharedStorageWorkletHost::GetLockManager(
    mojo::PendingReceiver<blink::mojom::LockManager> receiver) {
  shared_storage_runtime_manager_->lock_manager().BindLockManager(
      shared_storage_origin_, std::move(receiver));

  navigator_locks_invoked_ = true;
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

void SharedStorageWorkletHost::MaybeFinishCreateWorklet() {
  CHECK(create_worklet_finished_callback_);

  if ((needs_data_origin_opt_in_ && !data_origin_opt_in_state_) ||
      !script_loading_state_) {
    CHECK(needs_data_origin_opt_in_);
    // We need to wait until the next time that this method is called, which
    // will happen when both the script loading and the request to retrieve the
    // /.well-known/shared-storage/trusted-origins JSON have completed.
    return;
  }

  CHECK(script_loading_state_);
  bool success = script_loading_state_->first;
  std::string error_message = script_loading_state_->second;

  if (needs_data_origin_opt_in_) {
    CHECK(data_origin_opt_in_state_);
    if (!data_origin_opt_in_state_->first) {
      success = false;

      // Set worklet to expire before any more operations can run, since we do
      // not have permission to access the data.
      keep_alive_after_operation_ = false;

      // Use error message for data opt-in failure if there isn't already a
      // script-loading error message.
      if (script_loading_state_->first) {
        error_message = data_origin_opt_in_state_->second;
      }
    }
  }

  std::move(create_worklet_finished_callback_).Run(success, error_message);

  DecrementPendingOperationsCount();
}

void SharedStorageWorkletHost::OnRunOperationOnWorkletFinished(
    base::TimeTicks run_start_time,
    base::TimeTicks execution_start_time,
    int operation_id,
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

  shared_storage_runtime_manager_->NotifyWorkletOperationExecutionFinished(
      base::TimeTicks::Now() - run_start_time, AccessMethod::kRun, operation_id,
      GetWorkletDevToolsToken(), GetMainFrameIdIfAvailable(),
      shared_storage_origin_.Serialize());

  base::TimeDelta time_in_worklet =
      base::TimeTicks::Now() - execution_start_time;

  base::UmaHistogramLongTimes(
      "Storage.SharedStorage.Document.Timing.Run.ExecutedInWorklet",
      time_in_worklet);

  dwa::builders::SharedStorage_RunFinishedInWorklet()
      .SetContent(shared_storage_origin_.Serialize())
      .SetTimeInWorklet(ukm::GetExponentialBucketMinForUserTiming(
          time_in_worklet.InMilliseconds()))
      .Record(metrics::dwa::DwaRecorder::Get());

  DecrementPendingOperationsCount();
}

void SharedStorageWorkletHost::
    OnRunURLSelectionOperationOnWorkletScriptExecutionFinished(
        const GURL& urn_uuid,
        base::TimeTicks select_url_start_time,
        base::TimeTicks execution_start_time,
        int operation_id,
        const std::string& operation_name,
        const std::u16string& saved_query_name_to_cache,
        bool success,
        const std::string& error_message,
        uint32_t index) {
  auto it = unresolved_urns_.find(urn_uuid);
  CHECK(it != unresolved_urns_.end());

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
                     weak_ptr_factory_.GetWeakPtr(), urn_uuid,
                     select_url_start_time, execution_start_time, operation_id,
                     operation_name, saved_query_name_to_cache, success,
                     error_message, index, /*use_page_budgets=*/true));
}

void SharedStorageWorkletHost::OnRunURLSelectionOperationOnWorkletFinished(
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
    BudgetResult budget_result) {
  auto it = unresolved_urns_.find(urn_uuid);
  CHECK(it != unresolved_urns_.end());

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
            std::move(urls_with_metadata), index, use_page_budgets,
            budget_result.bits, budget_status);

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
            blink::mojom::ConsoleMessageLevel::kInfo,
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

    shared_storage_runtime_manager_->NotifyConfigPopulated(config);

    // If the query is named and not previously cached, cache the query's
    // `index` for later and run any callbacks stored to make use of this
    // result.
    if (saved_queries_enabled_ && !saved_query_name_to_cache.empty()) {
      page_->SetSavedQueryResultIndexAndRunCallbacks(
          shared_storage_origin_, script_source_url_, operation_name,
          saved_query_name_to_cache, index);
    }
  } else {
    LogSharedStorageWorkletError(
        blink::SharedStorageWorkletErrorType::kSelectURLWebVisible);
  }

  shared_storage_runtime_manager_->NotifyWorkletOperationExecutionFinished(
      base::TimeTicks::Now() - select_url_start_time, AccessMethod::kSelectURL,
      operation_id, GetWorkletDevToolsToken(), GetMainFrameIdIfAvailable(),
      shared_storage_origin_.Serialize());

  base::TimeDelta time_in_worklet =
      base::TimeTicks::Now() - execution_start_time;

  base::UmaHistogramLongTimes(
      "Storage.SharedStorage.Document.Timing.SelectURL.ExecutedInWorklet",
      time_in_worklet);

  dwa::builders::SharedStorage_SelectUrlFinishedInWorklet()
      .SetContent(shared_storage_origin_.Serialize())
      .SetTimeInWorklet(ukm::GetExponentialBucketMinForUserTiming(
          time_in_worklet.InMilliseconds()))
      .Record(metrics::dwa::DwaRecorder::Get());

  DecrementPendingOperationsCount();
}

void SharedStorageWorkletHost::OnSelectURLSavedQueryFound(
    const GURL& urn_uuid,
    base::TimeTicks select_url_start_time,
    base::TimeTicks execution_start_time,
    int operation_id,
    const std::string& operation_name,
    uint32_t index) {
  auto it = unresolved_urns_.find(urn_uuid);
  CHECK(it != unresolved_urns_.end());

  if (index >= it->second.size()) {
    // Return the default index if the saved index is out-of-range for the
    // current vector of URLs.
    index = 0;
  }

  shared_storage_manager_->GetRemainingBudget(
      shared_storage_site_,
      base::BindOnce(&SharedStorageWorkletHost::
                         OnRunURLSelectionOperationOnWorkletFinished,
                     weak_ptr_factory_.GetWeakPtr(), urn_uuid,
                     select_url_start_time, execution_start_time, operation_id,
                     operation_name,
                     /*saved_query_name_to_cache=*/std::u16string(),
                     /*script_execution_succeeded=*/true,
                     /*script_execution_error_message=*/std::string(), index,
                     /*use_page_budgets=*/false));
}

void SharedStorageWorkletHost::ExpireWorklet() {
  // `this` is not in keep-alive.
  DCHECK(document_service_);
  DCHECK(shared_storage_runtime_manager_);

  // This will remove this worklet host from the manager.
  shared_storage_runtime_manager_->ExpireWorkletHostForDocumentService(
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

  if (pending_operations_count_) {
    return;
  }

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

    // Only supply a CodeCacheHost when PersistentCache is not enabled, as
    // Shared Storage is deprecated and its removal is planned.
    mojo::PendingRemote<blink::mojom::CodeCacheHost> proxied_code_cache_host;
    if (!blink::features::IsPersistentCacheForCodeCacheEnabled()) {
      mojo::PendingRemote<blink::mojom::CodeCacheHost> actual_code_cache_host;
      code_cache_host_receivers_->Add(
          rfh.GetProcess()->GetDeprecatedID(), rfh.GetNetworkIsolationKey(),
          rfh.GetStorageKey(),
          actual_code_cache_host.InitWithNewPipeAndPassReceiver());

      code_cache_host_proxy_ =
          std::make_unique<SharedStorageCodeCacheHostProxy>(
              std::move(actual_code_cache_host),
              proxied_code_cache_host.InitWithNewPipeAndPassReceiver(),
              script_source_url_);
    }

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

    blink::mojom::SharedStorageWorkletPermissionsPolicyStatePtr
        permissions_policy_state =
            GetSharedStorageWorkletPermissionsPolicyState(
                rfh, shared_storage_origin_);

    auto embedder_context = static_cast<RenderFrameHostImpl&>(
                                document_service_->render_frame_host())
                                .frame_tree_node()
                                ->GetEmbedderSharedStorageContextIfAllowed();

    shared_storage_worklet_service_->Initialize(
        shared_storage_worklet_service_client_.BindNewEndpointAndPassRemote(),
        std::move(permissions_policy_state), embedder_context,
        base::BindOnce(&SharedStorageWorkletHost::OnWorkletInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
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

  std::optional<base::TimeDelta> timeout;
  if (PrivateAggregationManager::ShouldSendReportDeterministically(
          PrivateAggregationCallerApi::kSharedStorage,
          private_aggregation_config->context_id,
          private_aggregation_config->filtering_id_max_bytes,
          static_cast<std::optional<size_t>>(
              private_aggregation_config->max_contributions))) {
    timeout = base::Seconds(5);
  }

  bool success = private_aggregation_manager->BindNewReceiver(
      shared_storage_origin_, main_frame_origin_,
      PrivateAggregationCallerApi::kSharedStorage,
      private_aggregation_config->context_id, std::move(timeout),
      private_aggregation_config->aggregation_coordinator_origin,
      private_aggregation_config->filtering_id_max_bytes,
      static_cast<std::optional<size_t>>(
          private_aggregation_config->max_contributions),
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

void SharedStorageWorkletHost::OnOptInRequestComplete(
    std::optional<std::string> response_body) {
  const auto* response_info = data_origin_opt_in_url_loader_->ResponseInfo();
  if (!response_body || !response_info ||
      !blink::IsJSONMimeType(response_info->mime_type)) {
    SetDataOriginOptInResultAndMaybeFinish(
        /*opted_in=*/false, /*data_origin_opt_in_error_message=*/base::StrCat(
            {"Unable to parse the /.well-known/shared-storage/trusted-origins "
             "file for ",
             shared_storage_origin_.Serialize(),
             " due to no response, an invalid response, ",
             "or an unexpected mime type."}));
    return;
  }

  // `data_origin_opt_in_url_loader_` is no longer needed after this point.
  data_origin_opt_in_url_loader_.reset();

  std::optional<base::Value::List> parsed =
      base::JSONReader::ReadList(*response_body, base::JSON_PARSE_RFC);
  OnJsonParsed(std::move(parsed));
}

void SharedStorageWorkletHost::OnJsonParsed(
    std::optional<base::Value::List> result) {
  std::string shared_storage_origin_str = shared_storage_origin_.Serialize();
  if (!result.has_value()) {
    SetDataOriginOptInResultAndMaybeFinish(
        /*opted_in=*/false, /*data_origin_opt_in_error_message=*/base::StrCat(
            {"Unable to parse the /.well-known/shared-storage/trusted-origins "
             "file for ",
             shared_storage_origin_str,
             " because there was no parse result or the result was not a "
             "list."}));
    return;
  }

  if (result->empty()) {
    SetDataOriginOptInResultAndMaybeFinish(
        /*opted_in=*/false, /*data_origin_opt_in_error_message=*/base::StrCat(
            {"The /.well-known/shared-storage/trusted-origins file for ",
             shared_storage_origin_str, "is an empty list."}));
    return;
  }

  bool script_origin_match = false;
  bool context_origin_match = false;
  url::Origin worklet_script_origin = url::Origin::Create(script_source_url_);
  for (const base::Value& item_value : result.value()) {
    if (!item_value.is_dict()) {
      SetDataOriginOptInResultAndMaybeFinish(
          /*opted_in=*/false, /*data_origin_opt_in_error_message=*/base::StrCat(
              {"Unable to parse the "
               "/.well-known/shared-storage/trusted-origins "
               "file for ",
               shared_storage_origin_str,
               " because a non-dictionary item was encountered."}));
      return;
    }
    const std::string* script_origin_string =
        item_value.GetDict().FindString("scriptOrigin");
    if (!script_origin_string) {
      const base::Value::List* script_origin_list =
          item_value.GetDict().FindList("scriptOrigin");
      if (!script_origin_list || script_origin_list->empty()) {
        SetDataOriginOptInResultAndMaybeFinish(
            /*opted_in=*/false,
            /*data_origin_opt_in_error_message=*/base::StrCat(
                {"Unable to parse the "
                 "/.well-known/shared-storage/trusted-origins "
                 "file for ",
                 shared_storage_origin_str,
                 " because a dictionary item's `scriptOrigin` key was not "
                 "found, or its value was an empty list."}));
        return;
      }
      for (const base::Value& origin_value : *script_origin_list) {
        if (!origin_value.is_string()) {
          continue;
        }
        if (origin_value.GetString() == kAsteriskWildcard ||
            worklet_script_origin.IsSameOriginWith(
                GURL(origin_value.GetString()))) {
          script_origin_match = true;
          break;
        }
      }
    }
    if (script_origin_string &&
        (*script_origin_string == kAsteriskWildcard ||
         worklet_script_origin.IsSameOriginWith(GURL(*script_origin_string)))) {
      script_origin_match = true;
    }
    if (!script_origin_match) {
      continue;
    }
    const std::string* context_origin_string =
        item_value.GetDict().FindString("contextOrigin");
    if (!context_origin_string) {
      const base::Value::List* context_origin_list =
          item_value.GetDict().FindList("contextOrigin");
      if (!context_origin_list || context_origin_list->empty()) {
        SetDataOriginOptInResultAndMaybeFinish(
            /*opted_in=*/false,
            /*data_origin_opt_in_error_message=*/base::StrCat(
                {"Unable to parse the "
                 "/.well-known/shared-storage/trusted-origins "
                 "file for ",
                 shared_storage_origin_str,
                 " because a dictionary item's `contextOrigin` key was not "
                 "found, or its value was an empty list."}));
        return;
      }
      for (const base::Value& origin_value : *context_origin_list) {
        if (!origin_value.is_string()) {
          continue;
        }
        if (origin_value.GetString() == kAsteriskWildcard ||
            creator_context_origin_.IsSameOriginWith(
                GURL(origin_value.GetString()))) {
          context_origin_match = true;
          break;
        }
      }
    }
    if (context_origin_string && (*context_origin_string == kAsteriskWildcard ||
                                  creator_context_origin_.IsSameOriginWith(
                                      GURL(*context_origin_string)))) {
      context_origin_match = true;
    }
    if (script_origin_match && context_origin_match) {
      SetDataOriginOptInResultAndMaybeFinish(
          /*opted_in=*/true,
          /*data_origin_opt_in_error_message=*/std::string());
      return;
    }
  }
  SetDataOriginOptInResultAndMaybeFinish(
      /*opted_in=*/false, /*data_origin_opt_in_error_message=*/base::StrCat(
          {"Access of data from ", shared_storage_origin_str,
           " by a worklet script at ", script_source_url_.spec(),
           " invoked by ", creator_context_origin_.Serialize(),
           " has not been allowed."}));
}

void SharedStorageWorkletHost::OnCreateWorkletScriptLoadingFinished(
    bool success,
    const std::string& error_message) {
  // After the initial script loading, accessing shared storage will be allowed.
  // We want to disable the communication with network and with the cache, to
  // prevent leaking shared storage data.
  //
  // Note: The last code cache message (i.e. `DidGenerateCacheableMetadata()`,
  // if any) could race with this `MaybeFinishCreateWorklet()` callback, as
  // they are from separate mojom channels. It could impact the utility (i.e.
  // the generated code is not stored when the race happens).
  //
  // TODO(crbug.com/341690728): Measure how often the race happens, and
  // rearchitect if necessary.
  url_loader_factory_proxy_.reset();
  code_cache_host_proxy_.reset();

  script_loading_state_ = std::make_pair(success, error_message);
  MaybeFinishCreateWorklet();
}

const base::UnguessableToken& SharedStorageWorkletHost::GetWorkletToken()
    const {
  return worklet_token_;
}

const net::NetworkIsolationKey&
SharedStorageWorkletHost::MaybeGetNetworkIsolationKey() const {
  if (document_service_) {
    return static_cast<RenderFrameHostImpl&>(
               document_service_->render_frame_host())
        .GetNetworkIsolationKey();
  }
  static const base::NoDestructor<net::NetworkIsolationKey> kEmptyNIK;
  return *kEmptyNIK;
}

void SharedStorageWorkletHost::OnWorkletInitialized(
    const blink::SharedStorageWorkletToken& token) {
  worklet_token_ = static_cast<const base::UnguessableToken&>(token);
}

void SharedStorageWorkletHost::SetDataOriginOptInResultAndMaybeFinish(
    bool opted_in,
    std::string data_origin_opt_in_error_message) {
  data_origin_opt_in_state_ =
      std::make_pair(opted_in, std::move(data_origin_opt_in_error_message));
  MaybeFinishCreateWorklet();
}

GlobalRenderFrameHostId SharedStorageWorkletHost::GetMainFrameIdIfAvailable()
    const {
  return document_service_ ? document_service_->main_frame_id()
                           : GlobalRenderFrameHostId();
}

const base::UnguessableToken&
SharedStorageWorkletHost::GetWorkletDevToolsToken() const {
  return devtools_handle_->devtools_token();
}

const base::UnguessableToken&
SharedStorageWorkletHost::GetWorkletDevToolsTokenForTesting() const {
  return GetWorkletDevToolsToken();
}

}  // namespace content
