// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_METRICS_H_
#define CONTENT_BROWSER_SMS_SMS_METRICS_H_

#include "content/browser/sms/sms_parser.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/sms/webotp_service_outcome.h"

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

// Records the status of parsing an incoming SMS when using the WebOTP API.
void RecordSmsParsingStatus(SmsParsingStatus status, ukm::SourceId source_id);

// Records the result of a call to navigator.credentials.get({otp}) using
// the same histogram as WebOTPService API to provide continuity with previous
// iterations of the API.
void RecordSmsOutcome(blink::WebOTPServiceOutcome outcome,
                      ukm::SourceId source_id,
                      ukm::UkmRecorder* ukm_recorder,
                      bool is_cross_origin_frame);

// Records the time from when the API is called to when the user successfully
// receives the SMS and presses verify to move on with the verification flow.
// This uses the same histogram as WebOTPService API to provide continuity with
// previous iterations of the API.
void RecordSmsSuccessTime(base::TimeDelta duration,
                          ukm::SourceId source_id,
                          ukm::UkmRecorder* ukm_recorder);

// Records the time from when the API is called to when the request is cancelled
// by the service due to duplicated requests or lack of delegate.
void RecordSmsCancelTime(base::TimeDelta duration);

// Records the time from when the API is called to when the user dismisses the
// infobar to abort SMS retrieval. This uses the same histogram as WebOTPService
// API to provide continuity with previous iterations of the API.
void RecordSmsUserCancelTime(base::TimeDelta duration,
                             ukm::SourceId source_id,
                             ukm::UkmRecorder* ukm_recorder);

// Records whether the web contents that receives the OTP is visible or not.
void RecordWebContentsVisibilityOnReceive(bool is_visible);

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_METRICS_H_
