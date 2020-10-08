// Copyright 2019 The Chromium Authors. All rights reserved.
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

void RecordDestroyedReason(blink::SmsReceiverDestroyedReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Sms.Receive.DestroyedReason", reason);
}

void RecordSmsParsingStatus(SmsParsingStatus status, ukm::SourceId source_id) {
  ukm::builders::SMSReceiver builder(source_id);
  builder.SetSmsParsingStatus(static_cast<int>(status));
  builder.Record(ukm::UkmRecorder::Get());

  base::UmaHistogramEnumeration("Blink.Sms.Receive.SmsParsingStatus", status);
}

}  // namespace content
