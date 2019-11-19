// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_METRICS_H_

#include <stdint.h>

#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/sms/sms_receiver_outcome.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace blink {

// Records the result of a call to the SMSReceiver API.
void RecordSMSOutcome(SMSReceiverOutcome outcome,
                      ukm::SourceId source_id,
                      ukm::UkmRecorder* ukm_recorder);

// Records the time from when the API is called to when the user successfully
// receives the SMS and presses continue to move on with the verification flow.
void RecordSMSSuccessTime(base::TimeDelta duration);

// Records the time from when the API is called to when the user presses the
// cancel button to abort SMS retrieval.
void RecordSMSCancelTime(base::TimeDelta duration);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_METRICS_H_
