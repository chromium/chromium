// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/ml_installability_promoter.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/proto/segmentation_platform.pb.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/metrics/site_manifest_metrics_task.h"
#include "components/webapps/browser/installable/metrics/site_quality_metrics_task.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_install_result_reporter.h"
#include "components/webapps/browser/webapps_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webapps {

namespace {
const char kDisableGuardrailsSwitch[] = "disable-ml-install-history-guardrails";

enum class ManifestUrlInvalid {
  kEmpty = 0,
  kInvalid = 1,
  kValid = 2,
  kMaxValue = kValid
};

}  // namespace

MLInstallabilityPromoter::~MLInstallabilityPromoter() {
  if (service_worker_context_) {
    service_worker_context_->RemoveObserver(this);
  }
}

bool MLInstallabilityPromoter::HasCurrentInstall() {
  return !!current_install_;
}

std::unique_ptr<MlInstallOperationTracker>
MLInstallabilityPromoter::RegisterCurrentInstallForWebContents(
    WebappInstallSource install_source) {
  CHECK(!current_install_)
      << "Only one installion can be happening at any given time.";
  std::unique_ptr<MlInstallOperationTracker> tracker =
      std::make_unique<MlInstallOperationTracker>(
          base::PassKey<MLInstallabilityPromoter>(), install_source);
  if (ml_result_reporter_) {
    tracker->OnMlResultForInstallation(
        base::PassKey<MLInstallabilityPromoter>(),
        std::move(ml_result_reporter_));
  }
  current_install_ = tracker->GetWeakPtr();
  return tracker;
}

void MLInstallabilityPromoter::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK_IS_TEST();
  sequenced_task_runner_ = task_runner;
}

void MLInstallabilityPromoter::StartPipeline(const GURL& validated_url) {
  CHECK(web_contents());
  CHECK_EQ(state_, MLPipelineState::kInactive);
  if (!validated_url.is_valid() ||
      url::Origin::Create(validated_url).opaque()) {
    return;
  }
  AppBannerManager* app_banner_manager =
      AppBannerManager::FromWebContents(web_contents());
  if (!app_banner_manager) {
    return;
  }

  if (app_banner_manager->TriggeringDisabledForTesting()) {
    return;
  }

  // Do not run the pipeline again if there is an operation tracker
  // already alive and already has an ML data reporter connected to it.
  if (current_install_ && current_install_->MLReporterAlreadyConnected()) {
    return;
  }

  app_banner_manager_ = app_banner_manager->GetWeakPtr();
  site_url_ = validated_url;

  CHECK(state_ == MLPipelineState::kInactive);
  state_ = MLPipelineState::kRunningMetricTasks;

  WebappsClient* client = WebappsClient::Get();
  site_install_metrics_.is_fully_installed =
      client->IsAppFullyInstalledForSiteUrl(web_contents()->GetBrowserContext(),
                                            site_url_);
  site_install_metrics_.is_partially_installed =
      client->IsAppPartiallyInstalledForSiteUrl(
          web_contents()->GetBrowserContext(), site_url_);

  site_quality_metrics_task_ = SiteQualityMetricsTask::CreateAndStart(
      site_url_, *web_contents(), *storage_partition_, *service_worker_context_,
      sequenced_task_runner_,
      base::BindOnce(&MLInstallabilityPromoter::OnDidCollectSiteQualityMetrics,
                     weak_factory_.GetWeakPtr()));

  site_manifest_metrics_task_ = SiteManifestMetricsTask::CreateAndStart(
      *web_contents(),
      base::BindOnce(&MLInstallabilityPromoter::OnDidGetManifestForCurrentURL,
                     weak_factory_.GetWeakPtr()));

  sequenced_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&MLInstallabilityPromoter::OnDidWaitForObserversToFire,
                     weak_factory_.GetWeakPtr()),
      kTimeToWaitForWebContentsObservers);
}

MLInstallabilityPromoter::MLInstallabilityPromoter(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<MLInstallabilityPromoter>(*web_contents),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      storage_partition_(
          web_contents->GetPrimaryMainFrame()->GetStoragePartition()),
      service_worker_context_(nullptr) {
  CHECK(storage_partition_);
  service_worker_context_ = storage_partition_->GetServiceWorkerContext();
  CHECK(service_worker_context_);
  service_worker_context_->AddObserver(this);
}

void MLInstallabilityPromoter::OnDidCollectSiteQualityMetrics(
    const SiteQualityMetrics& site_quality_metrics) {
  site_quality_metrics_ = std::move(site_quality_metrics);
  site_quality_metrics_task_.reset();
  MaybeCompleteMetricsCollection();
}

void MLInstallabilityPromoter::OnDidGetManifestForCurrentURL(
    blink::mojom::ManifestPtr manifest) {
  manifest_ = std::move(manifest);
  site_manifest_metrics_task_.reset();
  MaybeCompleteMetricsCollection();
}

void MLInstallabilityPromoter::OnDidWaitForObserversToFire() {
  is_timeout_complete_ = true;
  MaybeCompleteMetricsCollection();
}

void MLInstallabilityPromoter::MaybeCompleteMetricsCollection() {
  if (site_manifest_metrics_task_ || site_quality_metrics_task_ ||
      !is_timeout_complete_) {
    // This allows us to reach a state in tests where both the site quality and
    // site metrics tasks have run but the timeout task has not, allowing
    // effective testing of update logic.
    if (IsTimeoutTaskOnlyPending() && run_loop_for_testing_) {
      CHECK_IS_TEST();
      run_loop_for_testing_->Quit();
    }
    return;
  }
  EmitUKMs();
}

void MLInstallabilityPromoter::AwaitMetricsCollectionTasksCompleteForTesting() {
  CHECK_IS_TEST();
  if (!site_manifest_metrics_task_ && !site_quality_metrics_task_) {
    return;
  }

  if (!run_loop_for_testing_) {
    run_loop_for_testing_ = std::make_unique<base::RunLoop>();
  }
  run_loop_for_testing_->Run();
  run_loop_for_testing_.reset();
}

GURL MLInstallabilityPromoter::GetProjectedManifestIdAfterMetricsCollection() {
  switch (state_) {
    case MLPipelineState::kInactive:
    case MLPipelineState::kRunningMetricTasks:
      CHECK(false) << "Cannot get manifest id without metrics collected";
      break;
    case MLPipelineState::kUKMCollectionComplete:
    case MLPipelineState::kMLClassificationRequested:
    case MLPipelineState::kWaitingForVisibility:
    case MLPipelineState::kComplete:
      break;
  }
  GURL manifest_id;
  if (blink::IsEmptyManifest(manifest_)) {
    manifest_id = site_url_.GetWithoutRef();
  } else {
    manifest_id = manifest_->id;
    if (!manifest_id.is_valid()) {
      manifest_id = site_url_.GetWithoutRef();
    }
  }
  CHECK(manifest_id.is_valid()) << " invalid manifest_id: " << manifest_id;
  return manifest_id;
}

void MLInstallabilityPromoter::EmitUKMs() {
  state_ = MLPipelineState::kUKMCollectionComplete;
  ukm::SourceId source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();

  // Record Site.Quality event data.
  ukm::builders::Site_Quality(source_id)
      .SetCacheStorageSize(ukm::GetExponentialBucketMinForBytes(
          site_quality_metrics_.cache_storage_size))
      .SetHasFavicons(site_quality_metrics_.non_default_favicons_count > 0)
      .SetHasFetchHandler(site_quality_metrics_.has_fetch_handler)
      .SetServiceWorkerScriptSize(ukm::GetExponentialBucketMinForBytes(
          site_quality_metrics_.service_worker_script_size))
      .Record(ukm_recorder->Get());

  // Record Site.Install Event data.
  ukm::builders::Site_Install(source_id)
      .SetIsFullyInstalled(site_install_metrics_.is_fully_installed)
      .SetIsPartiallyInstalled(site_install_metrics_.is_partially_installed)
      .Record(ukm_recorder->Get());

  // Record Site.Manifest Event data.
  ukm::builders::Site_Manifest manifest_builder(source_id);
  if (blink::IsEmptyManifest(manifest_)) {
    // See NullableBoolean in enums.xml for more information.
    manifest_builder
        .SetDisplayMode(
            -1)  // Denotes that it is empty because the manifest is missing.
        .SetHasBackgroundColor(/*NullableBoolean::Null=*/2)
        .SetHasIconsAny(/*NullableBoolean::Null=*/2)
        .SetHasIconsMaskable(/*NullableBoolean::Null=*/2)
        .SetHasName(/*NullableBoolean::Null=*/2)
        .SetHasScreenshots(/*NullableBoolean::Null=*/2)
        .SetHasStartUrl(
            -1)  // See ManifestUrlValidity in enums.xml for more information.
        .SetHasThemeColor(/*NullableBoolean::Null=*/2);
  } else {
    manifest_builder.SetDisplayMode(static_cast<int>(manifest_->display))
        .SetHasBackgroundColor(manifest_->has_background_color)
        .SetHasName(manifest_->name.has_value())
        .SetHasScreenshots(!manifest_->screenshots.empty())
        .SetHasThemeColor(manifest_->has_theme_color);

    // Set icon data in the UKM.
    bool has_manifest_icons_any = false;
    bool has_manifest_icons_maskable = false;
    for (const auto& icon : manifest_->icons) {
      for (const auto manifest_purpose : icon.purpose) {
        if (manifest_purpose ==
            blink::mojom::ManifestImageResource_Purpose::ANY) {
          has_manifest_icons_any = true;
        }
        if (manifest_purpose ==
            blink::mojom::ManifestImageResource_Purpose::MASKABLE) {
          has_manifest_icons_maskable = true;
        }
      }
      if (has_manifest_icons_any && has_manifest_icons_maskable) {
        break;
      }
    }
    manifest_builder.SetHasIconsAny(has_manifest_icons_any)
        .SetHasIconsMaskable(has_manifest_icons_maskable);

    // Set Manifest start URL data in UKM.
    if (!manifest_->has_valid_specified_start_url) {
      manifest_builder.SetHasStartUrl(
          static_cast<int>(ManifestUrlInvalid::kEmpty));
    } else if (manifest_->start_url.is_valid()) {
      manifest_builder.SetHasStartUrl(
          static_cast<int>(ManifestUrlInvalid::kValid));
    } else {
      manifest_builder.SetHasStartUrl(
          static_cast<int>(ManifestUrlInvalid::kInvalid));
    }
  }
  manifest_builder.Record(ukm_recorder->Get());
  RequestMlClassification();
}

void MLInstallabilityPromoter::RequestMlClassification() {
  CHECK_EQ(state_, MLPipelineState::kUKMCollectionComplete);
  state_ = MLPipelineState::kMLClassificationRequested;

  if (!app_banner_manager_) {
    state_ = MLPipelineState::kComplete;
    return;
  }
  WebappsClient* client = WebappsClient::Get();
  segmentation_platform::SegmentationPlatformService* segmentation =
      client->GetSegmentationPlatformService(
          web_contents()->GetBrowserContext());
  if (!segmentation || !base::FeatureList::IsEnabled(
                           features::kWebAppsEnableMLModelForPromotion)) {
    state_ = MLPipelineState::kComplete;
    return;
  }
  if (client->IsAppFullyInstalledForSiteUrl(web_contents()->GetBrowserContext(),
                                            site_url_) ||
      client->IsInAppBrowsingContext(web_contents())) {
    // Finish the pipeline early if an app is installed here.
    state_ = MLPipelineState::kComplete;
    return;
  }
  if ((!manifest_ || !manifest_->has_valid_specified_start_url) &&
      WebappsClient::Get()->IsUrlControlledBySeenManifest(
          web_contents()->GetBrowserContext(), site_url_)) {
    state_ = MLPipelineState::kComplete;
    return;
  }

  auto input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  input_context->metadata_args = {
      {"origin", segmentation_platform::processing::ProcessedValue(
                     url::Origin::Create(site_url_).GetURL())},
      {"site_url",
       segmentation_platform::processing::ProcessedValue(site_url_)},
      {"manifest_id", segmentation_platform::processing::ProcessedValue(
                          GetProjectedManifestIdAfterMetricsCollection())}};
  segmentation_platform::PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;
  segmentation->GetClassificationResult(
      segmentation_platform::kWebAppInstallationPromoKey, prediction_options,
      input_context,
      base::BindOnce(&MLInstallabilityPromoter::OnClassificationResult,
                     weak_factory_.GetWeakPtr()));
}

void MLInstallabilityPromoter::OnClassificationResult(
    const segmentation_platform::ClassificationResult& result) {
  CHECK_EQ(state_, MLPipelineState::kMLClassificationRequested);
  state_ = MLPipelineState::kComplete;
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    return;
  }
  // TODO(crbug.com/40272826) Remove this.
  if (!app_banner_manager_) {
    // Exit pipeline early if the AppBannerManager is destroyed.
    return;
  }
  WebappsClient* client = WebappsClient::Get();
  if (client->IsAppFullyInstalledForSiteUrl(web_contents()->GetBrowserContext(),
                                            site_url_)) {
    // An installation could have occurred while executing the ML logic.
    return;
  }
  GURL manifest_id = GetProjectedManifestIdAfterMetricsCollection();
  bool has_icons = site_quality_metrics_.non_default_favicons_count > 0 ||
                   !manifest_->icons.empty();
  bool blocked_by_history_guardrails =
      client->IsMlPromotionBlockedByHistoryGuardrail(
          web_contents()->GetBrowserContext(), manifest_id);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableGuardrailsSwitch)) {
    blocked_by_history_guardrails = false;
  }
  // Promotion from this Ml result is blocked by guardrails if it doesn't have
  // any icons, or if there has been a history of recent ignores. See the
  // implementation of IsMlPromotionBlockedByHistoryGuardrail per platform for
  // more details.
  bool is_ml_promotion_blocked_by_guardrails =
      !has_icons || blocked_by_history_guardrails;
  ml_result_reporter_ = std::make_unique<MlInstallResultReporter>(
      web_contents()->GetBrowserContext()->GetWeakPtr(), result.request_id,
      result.ordered_labels[0], manifest_id,
      is_ml_promotion_blocked_by_guardrails);

  if (current_install_) {
    current_install_->OnMlResultForInstallation(
        base::PassKey<MLInstallabilityPromoter>(),
        std::move(ml_result_reporter_));
    return;
  }

  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    state_ = MLPipelineState::kWaitingForVisibility;
    return;
  }
  MaybeReportResultToAppBannerManager();
}

void MLInstallabilityPromoter::MaybeReportResultToAppBannerManager() {
  if (state_ != MLPipelineState::kComplete || !ml_result_reporter_ ||
      ml_result_reporter_->ml_promotion_blocked_by_guardrail() ||
      !app_banner_manager_) {
    // TODO(crbug.com/40272826) Remove the app_banner_manager check
    return;
  }
  app_banner_manager_->OnMlInstallPrediction(
      base::PassKey<MLInstallabilityPromoter>(),
      ml_result_reporter_->output_label());
}

void MLInstallabilityPromoter::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument()) {
    return;
  }
  // Reset the pipeline as early as possible in case the DidFinishLoad call has
  // a lot of subresources to wait for, etc.
  ResetRunningStagesAndTasksMaybeReportResult();
  if (handle->IsServedFromBackForwardCache()) {
    StartPipeline(site_url_);
  }
}

void MLInstallabilityPromoter::DidFinishLoad(
    content::RenderFrameHost* /*render_frame_host*/,
    const GURL& /*validated_url*/ url) {
  ResetRunningStagesAndTasksMaybeReportResult();
  StartPipeline(url);
}

void MLInstallabilityPromoter::OnVisibilityChanged(
    content::Visibility visibility) {
  if (state_ != MLPipelineState::kWaitingForVisibility ||
      visibility != content::Visibility::VISIBLE) {
    return;
  }
  state_ = MLPipelineState::kComplete;
  MaybeReportResultToAppBannerManager();
}

// Stop collecting data if the web contents have been destroyed.
void MLInstallabilityPromoter::WebContentsDestroyed() {
  Observe(nullptr);
  ResetRunningStagesAndTasksMaybeReportResult();
}

void MLInstallabilityPromoter::DidUpdateWebManifestURL(
    content::RenderFrameHost* rfh,
    const GURL& manifest_url) {
  // For all other states_, either the data collection has not started yet or it
  // has completed and the ML model has been triggered with the new data.
  if (state_ != MLPipelineState::kRunningMetricTasks) {
    return;
  }

  site_manifest_metrics_task_ = SiteManifestMetricsTask::CreateAndStart(
      *web_contents(),
      base::BindOnce(&MLInstallabilityPromoter::OnDidGetManifestForCurrentURL,
                     weak_factory_.GetWeakPtr()));
}

void MLInstallabilityPromoter::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  if (state_ != MLPipelineState::kRunningMetricTasks) {
    return;
  }

  // Only count favicon URLs that are not the default one set by the renderer in
  // the absence of icons in the html. Default URLs follow the
  // <document_origin>/favicon.ico format.
  for (const auto& favicon_urls : candidates) {
    if (!favicon_urls->is_default_icon) {
      ++site_quality_metrics_.non_default_favicons_count;
    }
  }
}

void MLInstallabilityPromoter::OnRegistrationStored(int64_t registration_id,
                                                    const GURL& scope) {
  if (!content::ServiceWorkerContext::ScopeMatches(scope, site_url_)) {
    return;
  }

  // For all other states_, either the data collection has not started yet or it
  // has completed and the ML model has been triggered with the new data.
  if (state_ != MLPipelineState::kRunningMetricTasks) {
    return;
  }

  // Restart the SiteQualityMetricsTask to read data from the QuotaManagerProxy
  // and the ServiceWorkerContext with registered service worker.
  site_quality_metrics_task_ = SiteQualityMetricsTask::CreateAndStart(
      site_url_, *web_contents(), *storage_partition_, *service_worker_context_,
      sequenced_task_runner_,
      base::BindOnce(&MLInstallabilityPromoter::OnDidCollectSiteQualityMetrics,
                     weak_factory_.GetWeakPtr()));
}

void MLInstallabilityPromoter::OnDestruct(
    content::ServiceWorkerContext* context) {
  if (site_quality_metrics_task_) {
    // If the service_worker_context shuts down in the middle of the call, reset
    // the task.
    site_quality_metrics_task_.reset();
  }
  service_worker_context_->RemoveObserver(this);
  service_worker_context_ = nullptr;
}

void MLInstallabilityPromoter::ResetRunningStagesAndTasksMaybeReportResult() {
  state_ = MLPipelineState::kInactive;
  site_url_ = GURL();
  // TODO(crbug.com/40272826) Remove this.
  app_banner_manager_.reset();
  site_manifest_metrics_task_.reset();
  site_quality_metrics_task_.reset();
  is_timeout_complete_ = false;
  // Note: Destroying this will report the result to the classification system,
  // if it wasn't given to an installation tracker.
  ml_result_reporter_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

bool MLInstallabilityPromoter::IsTimeoutTaskOnlyPending() {
  return !site_manifest_metrics_task_ && !site_quality_metrics_task_ &&
         !is_timeout_complete_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MLInstallabilityPromoter);

}  // namespace webapps
