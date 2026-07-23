// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_SWITCHER_METRICS_APP_SWITCHER_METRICS_H_
#define IOS_CHROME_BROWSER_APP_SWITCHER_METRICS_APP_SWITCHER_METRICS_H_

namespace base {
class TimeDelta;
}  // namespace base

// Records that the user entered the AI Summarization flow via App Switcher.
void RecordAppSwitcherAISummarizationEntrypoint();

// Records the outcome of fetching App Switcher params.
void RecordAppSwitcherFetchOutcome(bool success);

// Records the duration of fetching App Switcher params.
void RecordAppSwitcherFetchDuration(base::TimeDelta duration);

#endif  // IOS_CHROME_BROWSER_APP_SWITCHER_METRICS_APP_SWITCHER_METRICS_H_
