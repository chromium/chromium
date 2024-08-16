// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_METRICS_MODEL_BROWSER_STATE_METRICS_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_METRICS_MODEL_BROWSER_STATE_METRICS_H_

class ProfileManagerIOS;
// TODO(crbug.com/358356195): Remove this typedef when this header is updated
// to use ProfileManagerIOS.
using ChromeBrowserStateManager = ProfileManagerIOS;

namespace profile_metrics {
struct Counts;
}  // namespace profile_metrics

// Counts and returns summary information about the browser states currently in
// the `manager`. This information is returned in the output variable
// `counts`. Assumes that all field of `counts` are set to zero before the call.
bool CountBrowserStateInformation(ChromeBrowserStateManager* manager,
                                  profile_metrics::Counts* counts);

void LogNumberOfBrowserStates(ChromeBrowserStateManager* manager);

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_METRICS_MODEL_BROWSER_STATE_METRICS_H_
