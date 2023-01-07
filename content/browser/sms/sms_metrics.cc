// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/metrics_utils.h"

namespace content {

void RecordSmsReceiveTime(base::TimeDelta duration, ukm::SourceId source_id) {
  ukm::builders::SMSReceiver builder(source_id);
  builder.SetTimeSmsReceiveMs(
      ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  builder.Record(ukm::UkmRecorder::Get());

  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeSmsReceive", duration);
}

void RecordCancelOnSuccessTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeCancelOnSuccess", duration);
}

void RecordContinueOnSuccessTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeContinueOnSuccess",
                             duration);
}

void RecordSmsParsingStatus(SmsParsingStatus status, ukm::SourceId source_id) {
  ukm::builders::SMSReceiver builder(source_id);
  builder.SetSmsParsingStatus(static_cast<int>(status));
  builder.Record(ukm::UkmRecorder::Get());

  base::UmaHistogramEnumeration("Blink.Sms.Receive.SmsParsingStatus", status);
}

void RecordSmsOutcome(blink::WebOTPServiceOutcome outcome,
                      ukm::SourceId source_id,
                      ukm::UkmRecorder* ukm_recorder,
                      bool is_cross_origin_frame) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Sms.Receive.Outcome", outcome);

  DCHECK_NE(source_id, ukm::kInvalidSourceId);
  DCHECK(ukm_recorder);

  ukm::builders::SMSReceiver builder(source_id);
  builder.SetOutcome(static_cast<int>(outcome))
      .SetIsCrossOriginFrame(is_cross_origin_frame);
  builder.Record(ukm_recorder);
}

void RecordSmsSuccessTime(base::TimeDelta duration,
                          ukm::SourceId source_id,
                          ukm::UkmRecorder* ukm_recorder) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeSuccess", duration);

  DCHECK_NE(source_id, ukm::kInvalidSourceId);
  DCHECK(ukm_recorder);

  ukm::builders::SMSReceiver builder(source_id);
  // Uses exponential bucketing for datapoints reflecting user activity.
  builder.SetTimeSuccessMs(
      ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  builder.Record(ukm_recorder);
}

void RecordSmsCancelTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeCancel", duration);
}

void RecordSmsUserCancelTime(base::TimeDelta duration,
                             ukm::SourceId source_id,
                             ukm::UkmRecorder* ukm_recorder) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeUserCancel", duration);

  DCHECK_NE(source_id, ukm::kInvalidSourceId);
  DCHECK(ukm_recorder);

  ukm::builders::SMSReceiver builder(source_id);
  // Uses exponential bucketing for datapoints reflecting user activity.
  builder.SetTimeUserCancelMs(
      ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  builder.Record(ukm_recorder);
}

void RecordWebContentsVisibilityOnReceive(bool is_visible) {
  base::UmaHistogramBoolean("Blink.Sms.WebContentsVisibleOnReceive",
                            is_visible);
}

}  // namespace content
