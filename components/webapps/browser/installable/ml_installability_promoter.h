// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALLABILITY_PROMOTER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALLABILITY_PROMOTER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/metrics/site_quality_metrics_task.h"
#include "components/webapps/browser/installable/ml_install_result_reporter.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class ServiceWorkerContext;
class StoragePartition;
class WebContents;
enum class Visibility;
}  // namespace content

namespace segmentation_platform {
struct ClassificationResult;
}  // namespace segmentation_platform

namespace webapps {
class AppBannerManager;
class MlInstallOperationTracker;
class SiteManifestMetricsTask;

constexpr base::TimeDelta kTimeToWaitForWebContentsObservers =
    base::Seconds(10);

struct SiteInstallMetrics {
  bool is_fully_installed;
  bool is_partially_installed;
};

// This class is used to measure metrics after page load and trigger a ML model
// to promote installability of a site.
// Note: This class only runs it's pipeline if the web contents also has an
// AppBannerManager constructed.
//
// To use this class, any type of user installation showing installation UX to
// the user MUST:
// - Create a `MlInstallOperationTracker` via
//   `RegisterCurrentInstallForWebContents(source)`.
// - Call the `ReportResult` method on that tracker when the user installs,
//   dismisses, or ignores the installation UX.
// - Destroy the tracker when the installation UX is complete (this allows
//   `HasCurrentInstall()` to return false again).
//
// If that is done properly, then this class will:
// - Gather metrics on the given page load (to be used by ML model).
// - Call the segmentation service via
//   `AppBannerManager::GetSegmentationPlatformService()` to request
//   classification for the `kWebAppInstallationPromoKey`.
// - Automatically report the result to Ml if an installation is already showing
//   (AKA triggered by the user or developer) and exit.
// - Automatically reject the classification if the
//   `AppBannerManager::IsMlPromotionBlockedByHistoryGuardrail` returns true,
//   and exit.
// - Wait for the web contents to be visible.
// - Finally call `AppBannerManager::OnMlInstallPrediction` if none of the above
//   cases exited early.
//
// The reporting of the ML results & updating of the guardrails is done through
// `MlInstallOperationTracker::ReportResult` method, but ONLY if the
// installation was triggered/received a classification from ML.
//
// The following methods are used from AppBannerManager:
// `IsAppFullyInstalledForSiteUrl`, `IsAppPartiallyInstalledForSiteUrl`,
// `SaveInstallation*`, `IsMlPromotionBlockedByHistoryGuardrail`,
// `OnMlInstallPrediction`, `GetSegmentationPlatformService`.
//
// Browsertests are located in
// chrome/browser/web_applications/ml_promotion_browsertest.cc
class MLInstallabilityPromoter
    : public content::WebContentsObserver,
      public content::ServiceWorkerContextObserver,
      public content::WebContentsUserData<MLInstallabilityPromoter> {
 public:
  static constexpr char kShowInstallPromptLabel[] = "ShowInstallPrompt";
  static constexpr char kDontShowLabel[] = "DontShow";

  ~MLInstallabilityPromoter() override;

  MLInstallabilityPromoter(const MLInstallabilityPromoter&) = delete;
  MLInstallabilityPromoter& operator=(const MLInstallabilityPromoter&) = delete;

  // Returns if the current web_contents has an existing install happening.
  bool HasCurrentInstall();

  std::unique_ptr<MlInstallOperationTracker>
  RegisterCurrentInstallForWebContents(WebappInstallSource install_source);

  // ------ Testing functionalities, only to be called from tests -----
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  void AwaitMetricsCollectionTasksCompleteForTesting();

  bool IsPendingVisibilityForTesting() const {
    return state_ == MLPipelineState::kWaitingForVisibility;
  }

  bool IsCompleteForTesting() const {
    return state_ == MLPipelineState::kComplete;
  }

 private:
  explicit MLInstallabilityPromoter(content::WebContents* web_contents);
  friend class content::WebContentsUserData<MLInstallabilityPromoter>;

  // Starts the pipeline. The state MUST be kInvalid.
  void StartPipeline(const GURL& validated_url);
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

  // Can only be called after metrics are collected.
  GURL GetProjectedManifestIdAfterMetricsCollection();
  void EmitUKMs();
  void RequestMlClassification();
  void OnClassificationResult(
      const segmentation_platform::ClassificationResult& result);
  void MaybeReportResultToAppBannerManager();

  // contents::WebContentsObserver overrides
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
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

  void ResetRunningStagesAndTasksMaybeReportResult();
  bool IsTimeoutTaskOnlyPending();

  enum class MLPipelineState {
    kInactive = 0,
    kRunningMetricTasks = 1,
    kUKMCollectionComplete = 2,
    kMLClassificationRequested = 3,
    kWaitingForVisibility = 4,
    kComplete = 5,
  } state_ = MLPipelineState::kInactive;

  // - All of the following variables are reset when the page navigates. -

  // These variables are set on page load.
  GURL site_url_;
  // TODO(crbug.com/40272826) Use raw_ptr when this class is owned by
  // AppBannerManager.
  base::WeakPtr<AppBannerManager> app_banner_manager_;

  // Variables & tasks used during metrics collection.
  std::unique_ptr<SiteQualityMetricsTask> site_quality_metrics_task_;
  std::unique_ptr<SiteManifestMetricsTask> site_manifest_metrics_task_;
  blink::mojom::ManifestPtr manifest_;
  SiteQualityMetrics site_quality_metrics_;
  SiteInstallMetrics site_install_metrics_;
  bool is_timeout_complete_ = false;

  // If populated, then an install is happening for the current web contents.
  base::WeakPtr<MlInstallOperationTracker> current_install_;
  // This is populated if the ML result has come back and no installation has
  // occured. This is moved into the MlInstallOperationTracker on its creation.
  std::unique_ptr<MlInstallResultReporter> ml_result_reporter_;

  // - The following variables are not reset on page navigation. -
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  raw_ptr<content::StoragePartition> storage_partition_;
  raw_ptr<content::ServiceWorkerContext> service_worker_context_;
  std::unique_ptr<base::RunLoop> run_loop_for_testing_;

  base::WeakPtrFactory<MLInstallabilityPromoter> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALLABILITY_PROMOTER_H_
