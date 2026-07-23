// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_SWITCHER_MODEL_APP_SWITCHER_PARAMS_REQUEST_STATUS_H_
#define IOS_CHROME_BROWSER_APP_SWITCHER_MODEL_APP_SWITCHER_PARAMS_REQUEST_STATUS_H_

// Represents the status of a request to fetch App Switcher parameters.
enum class AppSwitcherParamsRequestStatus {
  kUnavailable,
  kRequested,
  kAvailable,
};

#endif  // IOS_CHROME_BROWSER_APP_SWITCHER_MODEL_APP_SWITCHER_PARAMS_REQUEST_STATUS_H_
