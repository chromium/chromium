// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"

namespace content {

AppCacheUpdateMetricsRecorder::AppCacheUpdateMetricsRecorder() = default;

void AppCacheUpdateMetricsRecorder::IncrementExistingCorruptionFixedInUpdate() {
#if DCHECK_IS_ON()
  DCHECK(!finalized_) << "UploadMetrics() already called";
#endif  // DCHECK_IS_ON()

  existing_corruption_fixed_in_update_++;
}

void AppCacheUpdateMetricsRecorder::IncrementExistingResourceCheck() {
#if DCHECK_IS_ON()
  DCHECK(!finalized_) << "UploadMetrics() already called";
#endif  // DCHECK_IS_ON()

  existing_resource_check_++;
}

void AppCacheUpdateMetricsRecorder::IncrementExistingResourceCorrupt() {
#if DCHECK_IS_ON()
  DCHECK(!finalized_) << "UploadMetrics() already called";
#endif  // DCHECK_IS_ON()

  existing_resource_corrupt_++;
}

void AppCacheUpdateMetricsRecorder::
    IncrementExistingResourceCorruptionRecovery() {
#if DCHECK_IS_ON()
  DCHECK(!finalized_) << "UploadMetrics() already called";
#endif  // DCHECK_IS_ON()

  existing_resource_corruption_recovery_++;
}

void AppCacheUpdateMetricsRecorder::IncrementExistingResourceNotCorrupt() {
#if DCHECK_IS_ON()
  DCHECK(!finalized_) << "UploadMetrics() already called";
#endif  // DCHECK_IS_ON()

  existing_resource_not_corrupt_++;
}

void AppCacheUpdateMetricsRecorder::IncrementExistingResourceReused() {
#if DCHECK_IS_ON()
  DCHECK(!finalized_) << "UploadMetrics() already called";
#endif  // DCHECK_IS_ON()

  existing_resource_reused_++;
}

void AppCacheUpdateMetricsRecorder::RecordCanceled() {
#if DCHECK_IS_ON()
  DCHECK(!finalized_) << "UploadMetrics() already called";
#endif  // DCHECK_IS_ON()

  canceled_ = true;
}

void AppCacheUpdateMetricsRecorder::RecordFinalInternalState(
    AppCacheUpdateJobState state) {
#if DCHECK_IS_ON()
  DCHECK(!finalized_) << "UploadMetrics() already called";
#endif  // DCHECK_IS_ON()

  final_internal_state_ = state;
}

void AppCacheUpdateMetricsRecorder::UploadMetrics() {
#if DCHECK_IS_ON()
  DCHECK(!finalized_) << "UploadMetrics() already called";
  finalized_ = true;
#endif  // DCHECK_IS_ON()

  base::UmaHistogramExactLinear(
      "appcache.UpdateJob.ExistingCorruptionFixedInUpdate",
      existing_corruption_fixed_in_update_, 50);
  base::UmaHistogramExactLinear("appcache.UpdateJob.ExistingResourceCheck",
                                existing_resource_check_, 50);
  base::UmaHistogramExactLinear("appcache.UpdateJob.ExistingResourceCorrupt",
                                existing_resource_corrupt_, 50);
  base::UmaHistogramExactLinear("appcache.UpdateJob.ExistingResourceNotCorrupt",
                                existing_resource_not_corrupt_, 50);
  base::UmaHistogramExactLinear("appcache.UpdateJob.ExistingResourceReused",
                                existing_resource_reused_, 50);
  base::UmaHistogramBoolean("appcache.UpdateJob.Canceled", canceled_);
  base::UmaHistogramEnumeration("appcache.UpdateJob.FinalInternalState",
                                final_internal_state_);

  if (existing_resource_corrupt_ > 0) {
    base::UmaHistogramExactLinear(
        "appcache.UpdateJob.ExistingResourceOnlyCorrupt",
        existing_resource_corrupt_, 50);
  }
  if (existing_resource_corruption_recovery_ > 0) {
    base::UmaHistogramExactLinear(
        "appcache.UpdateJob.ExistingResourceCorruptionRecovery",
        existing_resource_corruption_recovery_, 50);
  }
  if (existing_resource_not_corrupt_ > 0) {
    base::UmaHistogramExactLinear(
        "appcache.UpdateJob.ExistingResourceOnlyNotCorrupt",
        existing_resource_not_corrupt_, 50);
  }
}

}  // namespace content
