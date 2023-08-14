// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/ml_install_operation_tracker.h"

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/ml_install_result_reporter.h"

namespace webapps {

MlInstallOperationTracker::MlInstallOperationTracker(
    base::PassKey<MLInstallabilityPromoter> pass_key,
    WebappInstallSource source)
    : source_(source) {}
MlInstallOperationTracker::~MlInstallOperationTracker() = default;

void MlInstallOperationTracker::OnMlResultForInstallation(
    base::PassKey<MLInstallabilityPromoter>,
    std::unique_ptr<MlInstallResultReporter> reporter) {
  CHECK(!reporter_);
  reporter_ = std::move(reporter);
  reporter_->OnInstallTrackerAttached(source_);
}

void MlInstallOperationTracker::ReportResult(MlInstallUserResponse response) {
  if (reporter_) {
    reporter_->ReportResult(source_, response);
  }
}

bool MlInstallOperationTracker::MLReporterAlreadyConnected() {
  return reporter_ != nullptr;
}

base::WeakPtr<MlInstallOperationTracker>
MlInstallOperationTracker::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace webapps
