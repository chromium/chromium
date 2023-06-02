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
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webapps {

MLInstallabilityPromoter::~MLInstallabilityPromoter() {
  if (service_worker_context_) {
    service_worker_context_->RemoveObserver(this);
  }
}

void MLInstallabilityPromoter::SetAwaitSiteResourcesLoadCallbackForTesting(
    base::OnceClosure await_site_resources_load_callback_for_testing) {
  CHECK_IS_TEST();
  await_site_resources_load_callback_for_testing_ =
      std::move(await_site_resources_load_callback_for_testing);
}

void MLInstallabilityPromoter::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  CHECK_IS_TEST();
  sequenced_task_runner_ = task_runner;
}

const blink::mojom::ManifestPtr&
MLInstallabilityPromoter::GetManifestForTesting() {
  CHECK_IS_TEST();
  return manifest_;
}

const SiteQualityMetrics&
MLInstallabilityPromoter::GetSiteQualityMetricsForTesting() {
  CHECK_IS_TEST();
  return site_quality_metrics_;
}

const SiteInstallMetrics&
MLInstallabilityPromoter::GetSiteInstallMetricsForTesting() {
  CHECK_IS_TEST();
  return site_install_metrics_;
}

void MLInstallabilityPromoter::StartGatheringMetricsForSiteUrl() {
  if (!base::FeatureList::IsEnabled(features::kWebAppsMlUkmCollection)) {
    return;
  }

  CHECK(web_contents());
  const GURL& site_url = web_contents()->GetLastCommittedURL();

  if (!site_url.is_valid() || url::Origin::Create(site_url).opaque()) {
    return;
  }

  site_url_ = site_url;

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
      *web_contents(), *storage_partition_, *service_worker_context_,
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

void MLInstallabilityPromoter::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument()) {
    return;
  }

  if (handle->IsServedFromBackForwardCache()) {
    Reset();
    StartGatheringMetricsForSiteUrl();
  }
}

void MLInstallabilityPromoter::DidFinishLoad(
    content::RenderFrameHost* /*render_frame_host*/,
    const GURL& /*validated_url*/ url) {
  Reset();
  StartGatheringMetricsForSiteUrl();
}

// Stop collecting data if the web contents have been destroyed.
void MLInstallabilityPromoter::WebContentsDestroyed() {
  Observe(nullptr);
  Reset();
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
      *web_contents(), *storage_partition_, *service_worker_context_,
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

void MLInstallabilityPromoter::MaybeCompleteMetricsCollection() {
  if (site_manifest_metrics_task_ || site_quality_metrics_task_ ||
      !is_timeout_complete_) {
    // This allows us to reach a state in tests where both the site quality and
    // site metrics tasks have run but the timeout task has not, allowing
    // effective testing of update logic.
    if (IsTimeoutTaskOnlyPending()) {
      if (await_site_resources_load_callback_for_testing_) {
        CHECK_IS_TEST();
        std::move(await_site_resources_load_callback_for_testing_).Run();
      }
    }
    return;
  }

  Reset();
  state_ = MLPipelineState::kComplete;
  // TODO(b/284157768): Start measuring UKMs from here.
}

void MLInstallabilityPromoter::Reset() {
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
