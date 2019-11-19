// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace content {

void RecordSmsReceiveTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeSmsReceive", duration);
}

void RecordCancelOnSuccessTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeCancelOnSuccess", duration);
}

void RecordContinueOnSuccessTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Blink.Sms.Receive.TimeContinueOnSuccess",
                             duration);
}

}  // namespace content
