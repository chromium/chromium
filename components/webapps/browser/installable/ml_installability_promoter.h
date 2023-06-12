// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALLABILITY_PROMOTER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALLABILITY_PROMOTER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/webapps/browser/installable/metrics/site_quality_metrics_task.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
class RenderFrameHost;
class NavigationHandle;
class ServiceWorkerContext;
class StoragePartition;
}  // namespace content

namespace webapps {

class SiteManifestMetricsTask;

constexpr base::TimeDelta kTimeToWaitForWebContentsObservers = base::Seconds(3);

// This class is used to measure metrics after page load and trigger a ML model
// to promote installability of a site.
//
// Browsertests are located in
// chrome/browser/web_applications/ml_promotion_browsertest.cc

struct SiteInstallMetrics {
  bool is_fully_installed;
  bool is_partially_installed;
};

class MLInstallabilityPromoter
    : public content::WebContentsObserver,
      public content::ServiceWorkerContextObserver,
      public content::WebContentsUserData<MLInstallabilityPromoter> {
 public:
  ~MLInstallabilityPromoter() override;

  MLInstallabilityPromoter(const MLInstallabilityPromoter&) = delete;
  MLInstallabilityPromoter& operator=(const MLInstallabilityPromoter&) = delete;

  // ------ Testing functionalities, only to be called from tests -----
  void SetAwaitTimeoutTaskPendingCallbackForTesting(
      base::OnceClosure await_timeout_task_pending_callback_for_testing);
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

 private:
  // TODO(b/280455638):Add new states based on design.
  enum class MLPipelineState {
    kInactive = 0,
    kRunningMetricTasks = 1,
    kUKMCollectionComplete = 2,
    kMaxValue = kUKMCollectionComplete,
  };

  explicit MLInstallabilityPromoter(content::WebContents* web_contents);
  friend class content::WebContentsUserData<MLInstallabilityPromoter>;

  // Functions to start gathering metrics for the site URL.
  void StartGatheringMetricsForSiteUrl();
  void OnDidCollectSiteQualityMetrics(
      const SiteQualityMetrics& site_quality_metrics);
  void OnDidGetManifestForCurrentURL(blink::mojom::ManifestPtr manifest);

  // This is used to delay the calling of MaybeCompleteMetricsCollection() for a
  // specific amount of seconds for changes in the web contents to modify data
  // required for training the ML model.
  void OnDidWaitForObserversToFire();

  // This proceeds the ML pipeline only if:
  // 1. All metric tasks are complete and data is obtained.
  // 2. We have waited for kTimeToWaitForWebContentsObservers for all web
  // contents changes to be properly measured.
  void MaybeCompleteMetricsCollection();
  void EmitUKMs();
  void TriggerMLModel();

  // contents::WebContentsObserver overrides
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void WebContentsDestroyed() override;
  void DidUpdateWebManifestURL(content::RenderFrameHost* rfh,
                               const GURL& manifest_url) override;
  void DidUpdateFaviconURL(
      content::RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;

  // content::ServiceWorkerContextObserver overrides
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  void ResetRunningStagesAndTasks();
  bool IsTimeoutTaskOnlyPending();

  // Tasks that are responsible for collecting data to be used for ML promotion.
  std::unique_ptr<SiteQualityMetricsTask> site_quality_metrics_task_;
  std::unique_ptr<SiteManifestMetricsTask> site_manifest_metrics_task_;

  // ---- Variables that are collected for use in the ML Model. ----
  blink::mojom::ManifestPtr manifest_;
  SiteQualityMetrics site_quality_metrics_;
  SiteInstallMetrics site_install_metrics_;

  // ---- Variables that determine the stage of the ML pipeline ----
  MLPipelineState state_ = MLPipelineState::kInactive;
  bool is_timeout_complete_ = false;
  GURL site_url_;

  base::OnceClosure await_timeout_task_pending_callback_for_testing_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  raw_ptr<content::StoragePartition> storage_partition_;
  raw_ptr<content::ServiceWorkerContext> service_worker_context_;

  base::WeakPtrFactory<MLInstallabilityPromoter> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALLABILITY_PROMOTER_H_
