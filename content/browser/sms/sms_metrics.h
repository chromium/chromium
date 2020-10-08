// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_METRICS_H_
#define CONTENT_BROWSER_SMS_SMS_METRICS_H_

#include "content/browser/sms/sms_parser.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/sms/sms_receiver_destroyed_reason.h"

namespace base {
class TimeDelta;
}

namespace content {

using SmsParsingStatus = SmsParser::SmsParsingStatus;

// Records the time from when a call to the API was made to when an SMS has been
// successfully received.
void RecordSmsReceiveTime(base::TimeDelta duration, ukm::SourceId source_id);

// Records the time from when a successful SMS was retrieved to when the user
// presses the Cancel button.
void RecordCancelOnSuccessTime(base::TimeDelta duration);

// Records the time from when a successful SMS was retrieved to when the user
// presses the Continue button.
void RecordContinueOnSuccessTime(base::TimeDelta duration);

void RecordDestroyedReason(blink::SmsReceiverDestroyedReason reason);

// Records the status of parsing an incoming SMS when using the WebOTP API.
void RecordSmsParsingStatus(SmsParsingStatus status, ukm::SourceId source_id);

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_METRICS_H_
