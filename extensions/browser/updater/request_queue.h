// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_REQUEST_QUEUE_H_
#define EXTENSIONS_BROWSER_UPDATER_REQUEST_QUEUE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"

namespace extensions {

// This class keeps track of a queue of requests, and contains the logic to
// retry requests with some backoff policy. Each request has a
// net::BackoffEntry instance associated with it.
//
// The general flow when using this class would be something like this:
//   - requests are queued up by calling ScheduleRequest.
//   - when a request is ready to be executed, RequestQueue removes the
//     request from the queue, assigns it as active request, and calls
//     the callback that was passed to the constructor.
//   - (optionally) when a request has completed unsuccessfully call
//     RetryRequest to put the request back in the queue, using the
//     backoff policy and minimum backoff delay to determine when to
//     next schedule this request.
//   - call reset_active_request() to indicate that the active request has
//     been dealt with.
//   - call StartNextRequest to schedule the next pending request (if any).
template <typename T>
class RequestQueue {
 public:
  struct Request {
    Request(std::unique_ptr<net::BackoffEntry> backoff_entry,
            std::unique_ptr<T> fetch)
        : backoff_entry(std::move(backoff_entry)), fetch(std::move(fetch)) {}

    int failure_count() { return backoff_entry->failure_count(); }
    std::unique_ptr<net::BackoffEntry> backoff_entry;
    std::unique_ptr<T> fetch;
  };

  class iterator;

  RequestQueue(net::BackoffEntry::Policy backoff_policy,
               const base::RepeatingClosure& start_request_callback);
  ~RequestQueue();

  // Returns the request that is currently being processed.
  T* active_request();

  // Returns the number of times the current request has been retried already.
  int active_request_failure_count();

  // Signals RequestQueue that processing of the current request has completed.
  Request reset_active_request();

  // Add the given request to the queue, and starts the next request if no
  // request is currently being processed.
  void ScheduleRequest(std::unique_ptr<T> request);

  // Add the request which already was in the queue, but we've decided to retry
  // it. The queue will take care of the retry backoff.
  void ScheduleRetriedRequest(Request request,
                              const base::TimeDelta& min_backoff_delay);

  bool empty() const;
  size_t size() const;

  // Returns the earliest release time of all requests currently in the queue.
  base::TimeTicks NextReleaseTime() const;

  // Starts the next request, if no request is currently active. This will
  // synchronously call the start_request_callback if the release time of the
  // earliest available request is in the past, otherwise it will call that
  // callback asynchronously after enough time has passed.
  void StartNextRequest();

  // Tell RequestQueue to put the current request back in the queue, after
  // applying the backoff policy to determine when to next try this request.
  // If the policy results in a backoff delay smaller than |min_backoff_delay|,
  // that delay is used instead.
  void RetryRequest(const base::TimeDelta& min_backoff_delay);

  iterator begin();
  iterator end();

  // Checks all pending requests in the queue for the given condition, removes
  // from the queue and returns the ones for which the condition returned true.
  std::vector<std::unique_ptr<T>> erase_if(
      const base::RepeatingCallback<bool(const T&)> condition);

  // Change the backoff policy used by the queue.
  void set_backoff_policy(net::BackoffEntry::Policy backoff_policy);

 private:
  // Compares the release time of two pending requests.
  static bool CompareRequests(const Request& a, const Request& b);

  // Pushes a request with a given backoff entry onto the queue.
  void PushImpl(Request request);

  // The backoff policy used to determine backoff delays.
  net::BackoffEntry::Policy backoff_policy_;

  // Callback to call when a new request has become the active request.
  base::RepeatingClosure start_request_callback_;

  // Priority queue of pending requests. Not using std::priority_queue since
  // the code needs to be able to iterate over all pending requests.
  base::circular_deque<Request> pending_requests_;

  // Active entry with its associated backoff.
  std::optional<Request> active_request_;

  // Timer to schedule calls to StartNextRequest, if the first pending request
  // hasn't passed its release time yet.
  base::OneShotTimer timer_;
};

// Iterator class that wraps a base::circular_deque<> iterator, only giving
// access to the actual request part of each item.
template <typename T>
class RequestQueue<T>::iterator {
 public:
  iterator() = default;

  T* operator*() { return it_->fetch.get(); }
  T* operator->() { return it_->fetch.get(); }
  iterator& operator++() {
    ++it_;
    return *this;
  }
  bool operator!=(const iterator& b) const { return it_ != b.it_; }

 private:
  friend class RequestQueue<T>;
  using Container = base::circular_deque<typename RequestQueue<T>::Request>;

  explicit iterator(const typename Container::iterator& it) : it_(it) {}

  typename Container::iterator it_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_REQUEST_QUEUE_H_
