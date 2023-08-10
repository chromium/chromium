// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_TIME_TRACKER_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_TIME_TRACKER_H_

#include <stdint.h>

#include <map>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"

// This class monitors how much delay extensions add to network requests
// by using the webRequest API.
class ExtensionWebRequestTimeTracker {
 public:
  ExtensionWebRequestTimeTracker();

  ExtensionWebRequestTimeTracker(const ExtensionWebRequestTimeTracker&) =
      delete;
  ExtensionWebRequestTimeTracker& operator=(
      const ExtensionWebRequestTimeTracker&) = delete;

  ~ExtensionWebRequestTimeTracker();

  // Records the time that a request was created.  |has_listener| will be true
  // if there is at least one webRequest listener registered.
  // |has_extra_headers_listener| will be true if there is at least one listener
  // with 'extraHeaders' in the extraInfoSpec.
  void LogRequestStartTime(int64_t request_id,
                           const base::TimeTicks& start_time,
                           bool has_listener,
                           bool has_extra_headers_listener);

  // Records the time at which an onBeforeRequest event was dispatched to
  // listeners.
  void LogBeforeRequestDispatchTime(int64_t request_id,
                                    base::TimeTicks dispatch_time);

  // Records the time at which an onBeforeRequest event received a response
  // from all blocking listeners and the responses have been handled. Only
  // called if there was at least one blocking listener.
  void LogBeforeRequestCompletionTime(int64_t request_id,
                                      base::TimeTicks completion_time);

  // Records the time at which Chrome started to evaluate declarativeNetRequest
  // rules at the beginning of a request.
  void LogBeforeRequestDNRStartTime(int64_t request_id,
                                    base::TimeTicks start_time);

  // Records the time at which Chrome has completed handling
  // declarativeNetRequest rules. Only called if at least one rule was applied.
  void LogBeforeRequestDNRCompletionTime(int64_t request_id,
                                         base::TimeTicks completion_time);

  // Records the time that a request either completed or encountered an error.
  void LogRequestEndTime(int64_t request_id, const base::TimeTicks& end_time);

  // Records an additional delay for the given request caused by all extensions
  // combined.
  void IncrementTotalBlockTime(int64_t request_id,
                               const base::TimeDelta& block_time);

  // Called when an extension has canceled the given request.
  void SetRequestCanceled(int64_t request_id);

  // Called when an extension has redirected the given request to another URL.
  void SetRequestRedirected(int64_t request_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTimeTrackerTest, Histograms);

  // Timing information for a single request.
  struct RequestTimeLog {
    base::TimeTicks request_start_time;
    base::TimeTicks before_request_listener_dispatch_time;
    base::TimeTicks before_request_dnr_start_time;
    base::TimeTicks before_request_dnr_completion_time;
    base::TimeTicks before_request_listener_completion_time;

    base::TimeDelta block_duration;

    bool has_listener = false;
    bool has_extra_headers_listener = false;

    RequestTimeLog();
    RequestTimeLog(const RequestTimeLog&) = delete;
    RequestTimeLog& operator=(const RequestTimeLog&) = delete;
    ~RequestTimeLog();
  };

  // Records UMA metrics for the given request and its end time.
  void AnalyzeLogRequest(const RequestTimeLog& log,
                         const base::TimeTicks& end_time);

  // A map of current request IDs to timing info for each request.
  std::map<int64_t, RequestTimeLog> request_time_logs_;
};

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_TIME_TRACKER_H_
