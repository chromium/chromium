// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_CONSTANTS_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_CONSTANTS_H_

namespace link_to_text {

// Number of milliseconds before timing out link generation requests.
extern const double kLinkGenerationTimeoutInMs;

// Number of seconds before timing out when checking preconditions. Uses a short
// timeout for the time that the main thread can spin waiting for a response,
// and a much longer timeout for the callbacks created by the WebState.
extern const double kPreconditionsTimeoutInSeconds;
extern const double kPreconditionsWebStateTimeoutInSeconds;

}  // namespace link_to_text

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_CONSTANTS_H_
