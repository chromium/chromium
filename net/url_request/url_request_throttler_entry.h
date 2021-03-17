// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_THROTTLER_ENTRY_H_
#define NET_URL_REQUEST_URL_REQUEST_THROTTLER_ENTRY_H_

#include <stdint.h>

#include <string>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request_throttler_entry_interface.h"

namespace net {

class URLRequestThrottlerManager;

// URLRequestThrottlerEntry represents an entry of URLRequestThrottlerManager.
// It analyzes requests of a specific URL over some period of time, in order to
// deduce the back-off time for every request.
// The back-off algorithm consists of two parts. Firstly, exponential back-off
// is used when receiving 5XX server errors or malformed response bodies.
// The exponential back-off rule is enforced by URLRequestHttpJob. Any
// request sent during the back-off period will be cancelled.
// Secondly, a sliding window is used to count recent requests to a given
// destination and provide guidance (to the application level only) on whether
// too many requests have been sent and when a good time to send the next one
// would be. This is never used to deny requests at the network level.
class NET_EXPORT URLRequestThrottlerEntry
    : public URLRequestThrottlerEntryInterface {
 public:
  // Sliding window period.
  static const int kDefaultSlidingWindowPeriodMs;

  // Maximum number of requests allowed in sliding window period.
  static const int kDefaultMaxSendThreshold;

  // Number of initial errors to ignore before starting exponential back-off.
  static const int kDefaultNumErrorsToIgnore;

  // Initial delay for exponential back-off.
  static const int kDefaultInitialDelayMs;

  // Factor by which the waiting time will be multiplied.
  static const double kDefaultMultiplyFactor;

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  static const double kDefaultJitterFactor;

  // Maximum amount of time we are willing to delay our request.
  static const int kDefaultMaximumBackoffMs;

  // Time after which the entry is considered outdated.
  static const int kDefaultEntryLifetimeMs;

  // The manager object's lifetime must enclose the lifetime of this object.
  URLRequestThrottlerEntry(URLRequestThrottlerManager* manager,
                           const std::string& url_id);

  // The life span of instances created with this constructor is set to
  // infinite, and the number of initial errors to ignore is set to 0.
  // It is only used by unit tests.
  URLRequestThrottlerEntry(URLRequestThrottlerManager* manager,
                           const std::string& url_id,
                           int sliding_window_period_ms,
                           int max_send_threshold,
                           int initial_backoff_ms,
                           double multiply_factor,
                           double jitter_factor,
                           int maximum_backoff_ms);

  // Used by the manager, returns true if the entry needs to be garbage
  // collected.
  bool IsEntryOutdated() const;

  // Causes this entry to never reject requests due to back-off.
  void DisableBackoffThrottling();

  // Causes this entry to NULL its manager pointer.
  void DetachManager();

  // Implementation of URLRequestThrottlerEntryInterface.
  bool ShouldRejectRequest(const URLRequest& request) const override;
  int64_t ReserveSendingTimeForNextRequest(
      const base::TimeTicks& earliest_time) override;
  base::TimeTicks GetExponentialBackoffReleaseTime() const override;
  void UpdateWithResponse(int status_code) override;
  void ReceivedContentWasMalformed(int response_code) override;

 protected:
  ~URLRequestThrottlerEntry() override;

  void Initialize();

  // Returns true if the given response code is considered a success for
  // throttling purposes.
  bool IsConsideredSuccess(int response_code);

  // Equivalent to TimeTicks::Now(), virtual to be mockable for testing purpose.
  virtual base::TimeTicks ImplGetTimeNow() const;

  // Retrieves the back-off entry object we're using. Used to enable a
  // unit testing seam for dependency injection in tests.
  virtual const BackoffEntry* GetBackoffEntry() const;
  virtual BackoffEntry* GetBackoffEntry();

  // Used by tests.
  base::TimeTicks sliding_window_release_time() const {
    return sliding_window_release_time_;
  }

  // Used by tests.
  void set_sliding_window_release_time(const base::TimeTicks& release_time) {
    sliding_window_release_time_ = release_time;
  }

  // Valid and immutable after construction time.
  BackoffEntry::Policy backoff_policy_;

 private:
  // Timestamp calculated by the sliding window algorithm for when we advise
  // clients the next request should be made, at the earliest. Advisory only,
  // not used to deny requests.
  base::TimeTicks sliding_window_release_time_;

  // A list of the recent send events. We use them to decide whether there are
  // too many requests sent in sliding window.
  base::queue<base::TimeTicks> send_log_;

  const base::TimeDelta sliding_window_period_;
  const int max_send_threshold_;

  // True if DisableBackoffThrottling() has been called on this object.
  bool is_backoff_disabled_;

  // Access it through GetBackoffEntry() to allow a unit test seam.
  BackoffEntry backoff_entry_;

  // Weak back-reference to the manager object managing us.
  URLRequestThrottlerManager* manager_;

  // Canonicalized URL string that this entry is for; used for logging only.
  std::string url_id_;

  NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestThrottlerEntry);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_THROTTLER_ENTRY_H_
