// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_
#define CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_

namespace base {
class TimeDelta;
}

namespace content {

// Records the time from when a call to the API was made to when the accounts
// dialog is shown.
void RecordShowAccountsDialogTime(base::TimeDelta duration);

// Records the time from when the accounts dialog is shown to when the user
// presses the Continue button.
void RecordContinueOnDialogTime(base::TimeDelta duration);

// Records the time from when the accounts dialog is shown to when the user
// closes the dialog without selecting any account.
void RecordCancelOnDialogTime(base::TimeDelta duration);

// Records the time from when the user presses the Continue button to when the
// idtoken response is received. Also records the overall time from when the API
// is called to when the idtoken response is received.
void RecordIdTokenResponseAndTurnaroundTime(
    base::TimeDelta id_token_response_time,
    base::TimeDelta turnaround_time);

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDCM_METRICS_H_
