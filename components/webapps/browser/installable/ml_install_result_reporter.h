// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALL_RESULT_REPORTER_H_
#define COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALL_RESULT_REPORTER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/segmentation_platform/public/trigger.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace webapps {
enum class MlInstallUserResponse;

// This class is responsible for reporting the result of the Ml installation
// classification prediction and updating any guardrail metrics if applicable.
//
// This class is expected to be destroyed when a navigation occurs, or anything
// else that would invalidate a given WebContents from being installable.
//
// On destruction, if no result was reported, the result "ignored" is sent to
// classification (or, if the guardrail was applied, "blocked by guardrails").
//
// The reason the 'guardrail' result is only reported when this class is
// destroyed is that this allows any user-initiated installation to still
// possibly occur & report a real result to classification.
class MlInstallResultReporter {
 public:
  // Public for tests. Reported to UMA, do not change values.
  enum class MlInstallResponse {
    kAccepted = 0,
    kIgnored = 1,
    kCancelled = 2,
    kBlockedGuardrails = 3,
    kReporterDestroyed = 4,
    kMaxValue = kReporterDestroyed
  };

  MlInstallResultReporter(
      base::WeakPtr<content::BrowserContext> browser_context,
      segmentation_platform::TrainingRequestId training_request,
      std::string ml_output_label,
      const GURL& manifest_id,
      bool ml_promotion_blocked_by_guardrail);
  ~MlInstallResultReporter();

  const std::string& output_label() const;

  bool ml_promotion_blocked_by_guardrail() const;

  // Called when attached to an MlInstallOperationTracker.
  void OnInstallTrackerAttached(WebappInstallSource install_source);

  // Called to report If this installation has an ML result on it (via
  // construction OR via OnMlResult call), then this function will report the
  // result to the ml training api, update guardrails if applicable, and emit
  // the UMA. This can only be called once, and subsequent calls are ignored.
  void ReportResult(WebappInstallSource install_source,
                    MlInstallUserResponse response);

 private:
  void ReportResultInternal(std::optional<WebappInstallSource> source,
                            MlInstallResponse response);

  const base::WeakPtr<content::BrowserContext> browser_context_;
  const segmentation_platform::TrainingRequestId training_request_;
  std::string ml_output_label_;
  const GURL manifest_id_;
  bool ml_promotion_blocked_by_guardrail_;
  bool reported_ = false;
  std::optional<WebappInstallSource> install_source_attached_ = std::nullopt;
};

}  // namespace webapps
#endif  // COMPONENTS_WEBAPPS_BROWSER_INSTALLABLE_ML_INSTALL_RESULT_REPORTER_H_
