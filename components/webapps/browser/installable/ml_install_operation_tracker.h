// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALL_OPERATION_TRACKER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALL_OPERATION_TRACKER_H_

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace webapps {
class AppBannerManager;
class MLInstallabilityPromoter;

// The values of this enum are used for UMA metrics, do not change them.
enum class MlInstallUserResponse {
  // The dialog was accepted by the user, and the install was initiated.
  kAccepted = 0,
  // The install dialog was ignored via navigating away, clicking on a new tab,
  // etc.
  kIgnored = 1,
  // The install dialog was explicitly canceled by the user.
  kCancelled = 2,
  // The promotion was blocked by guardrails.
  kBlockedGuardrails = 3,
  kMaxValue = kBlockedGuardrails
};

// Reports the result of an install to the ML metrics system, if applicable.
// This is expected to be constructed via
// MLInstallabilityPromoter::RegisterCurrentInstallForWebContents, called from
// web_app_dialog_utils.h functions, so that all user-prompt installs for a
// given web contents are tracked this way.
class MlInstallOperationTracker {
 public:
  // This can only be constructed from the MLInstallabilityPromoter class.
  MlInstallOperationTracker(base::PassKey<MLInstallabilityPromoter>,
                            WebappInstallSource install_source);
  ~MlInstallOperationTracker();

  // This can only be called from the MLInstallabilityPromoter class.
  void OnMlResultForInstallation(
      base::PassKey<MLInstallabilityPromoter>,
      base::WeakPtr<AppBannerManager> app_banner_manager,
      segmentation_platform::TrainingRequestId training_request);

  // If this installation has an ML result on it (via construction OR via
  // OnMlResult call), then this function will report the result to the ml
  // training api, update guardrails if applicable, and emit the UMA.
  // This can only be called once, and subsequent calls are ignored.
  void ReportResult(const GURL& manifest_id, MlInstallUserResponse response);

  base::WeakPtr<MlInstallOperationTracker> GetWeakPtr();

 private:
  const WebappInstallSource install_source_;

  base::WeakPtr<AppBannerManager> app_banner_manager_;
  absl::optional<segmentation_platform::TrainingRequestId> training_request_;

  base::WeakPtrFactory<MlInstallOperationTracker> weak_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALL_OPERATION_TRACKER_H_
