// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sms/sms_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace blink {

void RecordSMSOutcome(SMSReceiverOutcome outcome,
                      ukm::SourceId source_id,
                      ukm::UkmRecorder* ukm_recorder) {
  DCHECK_NE(source_id, ukm::kInvalidSourceId);
  DCHECK(ukm_recorder);

  ukm::builders::SMSReceiver builder(source_id);
  builder.SetOutcome(static_cast<int>(outcome));
  builder.Record(ukm_recorder);

  UMA_HISTOGRAM_ENUMERATION("Blink.Sms.Receive.Outcome", outcome);
}

void RecordSMSSuccessTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeSuccess", duration);
}

void RecordSMSCancelTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeCancel", duration);
}

}  // namespace blink
