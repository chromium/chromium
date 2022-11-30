// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_THROTTLE_ENTRY_H_
#define EXTENSIONS_RENDERER_EXTENSION_THROTTLE_ENTRY_H_

#include <stdint.h>

#include <string>

#include "base/containers/queue.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"

namespace extensions {

// ExtensionThrottleEntry represents an entry of ExtensionThrottleManager.
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
class ExtensionThrottleEntry {
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

  // |url_id| is a unique entry ID.
  explicit ExtensionThrottleEntry(const std::string& url_id);

  // The life span of instances created with this constructor is set to
  // infinite, and the number of initial errors to ignore is set to 0.
  // It is only used by unit tests.
  ExtensionThrottleEntry(const std::string& url_id,
                         const net::BackoffEntry::Policy* backoff_policy);

  ExtensionThrottleEntry(const ExtensionThrottleEntry&) = delete;
  ExtensionThrottleEntry& operator=(const ExtensionThrottleEntry&) = delete;

  virtual ~ExtensionThrottleEntry();

  // Used by the manager, returns true if the entry needs to be garbage
  // collected.
  bool IsEntryOutdated() const;

  // Causes this entry to never reject requests due to back-off.
  void DisableBackoffThrottling();

  // Returns true when we have encountered server errors and are doing
  // exponential back-off.
  bool ShouldRejectRequest() const;

  // Calculates a recommended sending time for the next request and reserves it.
  // The sending time is not earlier than the current exponential back-off
  // release time or |earliest_time|. Moreover, the previous results of
  // the method are taken into account, in order to make sure they are spread
  // properly over time.
  // Returns the recommended delay before sending the next request, in
  // milliseconds. The return value is always positive or 0.
  // Although it is not mandatory, respecting the value returned by this method
  // is helpful to avoid traffic overload.
  int64_t ReserveSendingTimeForNextRequest(
      const base::TimeTicks& earliest_time);

  // Returns the time after which requests are allowed.
  base::TimeTicks GetExponentialBackoffReleaseTime() const;

  // This method needs to be called each time a response is received.
  void UpdateWithResponse(int status_code);

  // Lets higher-level modules, that know how to parse particular response
  // bodies, notify of receiving malformed content for the given URL. This will
  // be handled by the throttler as if an HTTP 503 response had been received to
  // the request, i.e. it will count as a failure, unless the HTTP response code
  // indicated is already one of those that will be counted as an error.
  void ReceivedContentWasMalformed(int response_code);

  // Get the URL ID associated with this entry. Should only be used for
  // debugging purpose.
  const std::string& GetURLIdForDebugging() const;

 protected:
  void Initialize();

  // Returns true if the given response code is considered a success for
  // throttling purposes.
  bool IsConsideredSuccess(int response_code);

  // Equivalent to TimeTicks::Now(), virtual to be mockable for testing purpose.
  virtual base::TimeTicks ImplGetTimeNow() const;

  // Retrieves the back-off entry object we're using. Used to enable a
  // unit testing seam for dependency injection in tests.
  virtual const net::BackoffEntry* GetBackoffEntry() const;
  virtual net::BackoffEntry* GetBackoffEntry();

  // Used by tests.
  base::TimeTicks sliding_window_release_time() const {
    return sliding_window_release_time_;
  }

  // Used by tests.
  void set_sliding_window_release_time(const base::TimeTicks& release_time) {
    sliding_window_release_time_ = release_time;
  }

  // Valid and immutable after construction time.
  net::BackoffEntry::Policy backoff_policy_;

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
  net::BackoffEntry backoff_entry_;

  // Canonicalized URL string that this entry is for; used for logging only.
  const std::string url_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_THROTTLE_ENTRY_H_
