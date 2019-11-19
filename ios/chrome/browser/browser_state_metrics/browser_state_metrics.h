// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_STATE_METRICS_BROWSER_STATE_METRICS_H_
#define IOS_CHROME_BROWSER_BROWSER_STATE_METRICS_BROWSER_STATE_METRICS_H_

namespace ios {
class ChromeBrowserStateManager;
}

namespace profile_metrics {
struct Counts;
enum class BrowserProfileType;
}  // namespace profile_metrics

namespace web {
class BrowserState;
}

// Counts and returns summary information about the browser states currently in
// the |manager|. This information is returned in the output variable
// |counts|. Assumes that all field of |counts| are set to zero before the call.
bool CountBrowserStateInformation(ios::ChromeBrowserStateManager* manager,
                                  profile_metrics::Counts* counts);

void LogNumberOfBrowserStates(ios::ChromeBrowserStateManager* manager);

profile_metrics::BrowserProfileType GetBrowserStateType(
    web::BrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_BROWSER_STATE_METRICS_BROWSER_STATE_METRICS_H_
