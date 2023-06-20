// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/ml_install_operation_tracker.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace webapps {

MlInstallOperationTracker::MlInstallOperationTracker(
    base::PassKey<MLInstallabilityPromoter> pass_key,
    WebappInstallSource install_source)
    : install_source_(install_source) {}
MlInstallOperationTracker::~MlInstallOperationTracker() = default;

void MlInstallOperationTracker::OnMlResultForInstallation(
    base::PassKey<MLInstallabilityPromoter>,
    base::WeakPtr<AppBannerManager> app_banner_manager,
    segmentation_platform::TrainingRequestId training_request) {
  CHECK(!training_request_);
  app_banner_manager_ = app_banner_manager;
  training_request_ = training_request;
}

void MlInstallOperationTracker::ReportResult(const GURL& manifest_id,
                                             MlInstallUserResponse response) {
  if (!training_request_ || !app_banner_manager_ ||
      !app_banner_manager_->GetSegmentationPlatformService()) {
    return;
  }
  CHECK(manifest_id.is_valid());
  base::UmaHistogramEnumeration("WebApp.MlInstall.DialogResponse", response);
  base::UmaHistogramEnumeration("WebApp.MlInstall.InstallSource",
                                install_source_, WebappInstallSource::COUNT);

  switch (response) {
    case MlInstallUserResponse::kAccepted:
      app_banner_manager_->SaveInstallationAcceptedForMl(manifest_id);
      break;
    case MlInstallUserResponse::kIgnored:
      app_banner_manager_->SaveInstallationIgnoredForMl(manifest_id);
      break;
    case MlInstallUserResponse::kCancelled:
      app_banner_manager_->SaveInstallationDismissedForMl(manifest_id);
      break;
    case MlInstallUserResponse::kBlockedGuardrails:
      break;
  }
  segmentation_platform::SegmentationPlatformService* segmentation =
      app_banner_manager_->GetSegmentationPlatformService();
  segmentation_platform::TrainingLabels training_labels;
  training_labels.output_metric =
      std::make_pair("WebApps.MlInstall.DialogResponse",
                     static_cast<base::HistogramBase::Sample>(response));
  segmentation->CollectTrainingData(
      segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_WEB_APP_INSTALLATION_PROMO,
      training_request_.value(), training_labels, base::DoNothing());
  // Training can only be reported once.
  training_request_ = absl::nullopt;
}

base::WeakPtr<MlInstallOperationTracker>
MlInstallOperationTracker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace webapps
