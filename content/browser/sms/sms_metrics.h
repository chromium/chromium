// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_METRICS_H_
#define CONTENT_BROWSER_SMS_SMS_METRICS_H_

namespace base {
class TimeDelta;
}

namespace content {

// Records the time from when a call to the API was made to when an SMS has been
// successfully received.
void RecordSmsReceiveTime(base::TimeDelta duration);

// Records the time from when a successful SMS was retrieved to when the user
// presses the Cancel button.
void RecordCancelOnSuccessTime(base::TimeDelta duration);

// Records the time from when a successful SMS was retrieved to when the user
// presses the Continue button.
void RecordContinueOnSuccessTime(base::TimeDelta duration);

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_METRICS_H_
