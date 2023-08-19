// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALL_OPERATION_TRACKER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALL_OPERATION_TRACKER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace webapps {
class MLInstallabilityPromoter;
class MlInstallResultReporter;

// These are the actions that a user can take when interacting with installation
// UX.
enum class MlInstallUserResponse {
  // The dialog was accepted by the user, and the install was initiated.
  kAccepted,
  // The install dialog was ignored via navigating away, clicking on a new tab,
  // etc.
  kIgnored,
  // The install dialog was explicitly canceled by the user.
  kCancelled,
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
                            WebappInstallSource source);
  ~MlInstallOperationTracker();

  // This can only be called from the MLInstallabilityPromoter class.
  void OnMlResultForInstallation(
      base::PassKey<MLInstallabilityPromoter>,
      std::unique_ptr<MlInstallResultReporter> reporter);

  // This call reports the result of the user's interaction with the
  // installation dialog to the MlInstallResultReporter, if it exists.
  void ReportResult(MlInstallUserResponse response);

  bool MLReporterAlreadyConnected();

  base::WeakPtr<MlInstallOperationTracker> GetWeakPtr();

 private:
  const WebappInstallSource source_;
  std::unique_ptr<MlInstallResultReporter> reporter_;

  base::WeakPtrFactory<MlInstallOperationTracker> weak_factory_{this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALL_OPERATION_TRACKER_H_
