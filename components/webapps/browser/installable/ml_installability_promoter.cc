// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/ml_installability_promoter.h"

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/metrics/site_manifest_metrics_task.h"
#include "components/webapps/browser/installable/metrics/site_quality_metrics_task.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webapps {

namespace {

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

void MLInstallabilityPromoter::SetAwaitTimeoutTaskPendingCallbackForTesting(
    base::OnceClosure await_site_resources_load_callback_for_testing) {
  CHECK_IS_TEST();
  await_timeout_task_pending_callback_for_testing_ =
      std::move(await_site_resources_load_callback_for_testing);
}

void MLInstallabilityPromoter::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK_IS_TEST();
  sequenced_task_runner_ = task_runner;
}

void MLInstallabilityPromoter::StartGatheringMetricsForSiteUrl() {
  CHECK(web_contents());
  const GURL& site_url = web_contents()->GetLastCommittedURL();

  if (!site_url.is_valid() || url::Origin::Create(site_url).opaque()) {
    return;
  }

  site_url_ = site_url;

  if (state_ != MLPipelineState::kInactive) {
    ResetRunningStagesAndTasks();
  }

  CHECK(state_ == MLPipelineState::kInactive);
  state_ = MLPipelineState::kRunningMetricTasks;

  AppBannerManager* app_banner_manager =
      AppBannerManager::FromWebContents(web_contents());
  if (app_banner_manager) {
    site_install_metrics_.is_fully_installed =
        app_banner_manager->IsAppFullyInstalledForSiteUrl(site_url);
    site_install_metrics_.is_partially_installed =
        app_banner_manager->IsAppPartiallyInstalledForSiteUrl(site_url);
  }

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
    if (IsTimeoutTaskOnlyPending()) {
      if (await_timeout_task_pending_callback_for_testing_) {
        CHECK_IS_TEST();
        std::move(await_timeout_task_pending_callback_for_testing_).Run();
      }
    }
    return;
  }

  ResetRunningStagesAndTasks();
  EmitUKMs();
}

void MLInstallabilityPromoter::EmitUKMs() {
  ukm::SourceId source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();

  // Record Site.Quality event data.
  ukm::builders::Site_Quality(source_id)
      .SetCacheStorageSize(ukm::GetExponentialBucketMinForBytes(
          site_quality_metrics_.cache_storage_size))
      .SetHasFavicons(site_quality_metrics_.favicons_count > 0)
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
    if (manifest_->start_url.is_empty()) {
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

  state_ = MLPipelineState::kUKMCollectionComplete;
  TriggerMLModel();
}

void MLInstallabilityPromoter::TriggerMLModel() {
  // TODO(b/283998203): Trigger the ML Model to start generating
  // insights based on the UKMs.
  CHECK_EQ(state_, MLPipelineState::kUKMCollectionComplete);

  if (!base::FeatureList::IsEnabled(
          features::kWebAppsEnableMLModelForPromotion)) {
    return;
  }
}

void MLInstallabilityPromoter::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument()) {
    return;
  }

  if (handle->IsServedFromBackForwardCache()) {
    ResetRunningStagesAndTasks();
    StartGatheringMetricsForSiteUrl();
  }
}

void MLInstallabilityPromoter::DidFinishLoad(
    content::RenderFrameHost* /*render_frame_host*/,
    const GURL& /*validated_url*/ url) {
  ResetRunningStagesAndTasks();
  StartGatheringMetricsForSiteUrl();
}

// Stop collecting data if the web contents have been destroyed.
void MLInstallabilityPromoter::WebContentsDestroyed() {
  Observe(nullptr);
  ResetRunningStagesAndTasks();
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

  site_quality_metrics_.favicons_count = candidates.size();
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

void MLInstallabilityPromoter::ResetRunningStagesAndTasks() {
  state_ = MLPipelineState::kInactive;
  site_manifest_metrics_task_.reset();
  site_quality_metrics_task_.reset();
  is_timeout_complete_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

bool MLInstallabilityPromoter::IsTimeoutTaskOnlyPending() {
  return !site_manifest_metrics_task_ && !site_quality_metrics_task_ &&
         !is_timeout_complete_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(MLInstallabilityPromoter);

}  // namespace webapps
