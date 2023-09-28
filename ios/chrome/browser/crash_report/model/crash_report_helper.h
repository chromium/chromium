// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORT_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORT_HELPER_H_

@class NSString;

namespace web {
class WebState;
}  // namespace web

class WebStateList;

namespace crash_report_helper {

// Monitors the urls loaded by `web_state` to allow crash reports to contain the
// current loading url.
// It is not needed to call that method for web_state contained in an already
// observed WebStateList. This method is typically used for Preload WebState
// (that are not in any WebStateList).
void MonitorURLsForPreloadWebState(web::WebState* web_state);

// Stop monitoring the urls loaded by `web_state`.
// If MonitorURLsForPreloadWebState(web_state) has been called, it is necessary
// to call StopMonitoringURLsForPreloadWebState before inserting the web_state
// in an observed WebStateList.
void StopMonitoringURLsForPreloadWebState(web::WebState* web_state);

// Monitors the urls loaded in `web_state_list` to allow crash reports to
// contain the currently loaded urls.
void MonitorURLsForWebStateList(WebStateList* web_state_list);

// Stop monitoring the urls loaded in the `web_state_list`.
void StopMonitoringURLsForWebStateList(WebStateList* web_state_list);

// Adds the state monitor to `web_state_list`. WebStateLists that are not
// monitored via this function are still monitored through WebStateObserver
// methods, but calling this function is mandatory to keep the monitoring of
// deleted tabs consistent.
void MonitorTabStateForWebStateList(WebStateList* web_state_list);

// Stop the state monitor of `web_state_list`.
void StopMonitoringTabStateForWebStateList(WebStateList* web_state_list);

// Clear any state about the urls loaded in the given WebStateList; this should
// be called when the WebStateList is deactivated.
void ClearStateForWebStateList(WebStateList* web_state_list);

}  // namespace crash_report_helper

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_REPORT_HELPER_H_
