// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_REQUEST_QUEUE_IMPL_H_
#define EXTENSIONS_BROWSER_UPDATER_REQUEST_QUEUE_IMPL_H_

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "extensions/browser/updater/request_queue.h"

namespace extensions {

template <typename T>
RequestQueue<T>::RequestQueue(
    net::BackoffEntry::Policy backoff_policy,
    const base::RepeatingClosure& start_request_callback)
    : backoff_policy_(backoff_policy),
      start_request_callback_(start_request_callback),
      active_request_(std::nullopt) {}

template <typename T>
RequestQueue<T>::~RequestQueue() = default;

template <typename T>
T* RequestQueue<T>::active_request() {
  return active_request_ ? active_request_->fetch.get() : nullptr;
}

template <typename T>
int RequestQueue<T>::active_request_failure_count() {
  DCHECK(active_request_);
  return active_request_->backoff_entry->failure_count();
}

template <typename T>
typename RequestQueue<T>::Request RequestQueue<T>::reset_active_request() {
  DCHECK(active_request_);
  Request request = std::move(*active_request_);
  active_request_.reset();
  return request;
}

template <typename T>
void RequestQueue<T>::ScheduleRequest(std::unique_ptr<T> request) {
  PushImpl(Request(std::unique_ptr<net::BackoffEntry>(
                       new net::BackoffEntry(&backoff_policy_)),
                   std::move(request)));
  StartNextRequest();
}

template <typename T>
void RequestQueue<T>::ScheduleRetriedRequest(
    Request request,
    const base::TimeDelta& min_backoff_delay) {
  DCHECK(request.backoff_entry);
  DCHECK(request.fetch);
  request.backoff_entry->InformOfRequest(false);
  if (request.backoff_entry->GetTimeUntilRelease() < min_backoff_delay) {
    request.backoff_entry->SetCustomReleaseTime(base::TimeTicks::Now() +
                                                min_backoff_delay);
  }
  PushImpl(std::move(request));
}

template <typename T>
void RequestQueue<T>::PushImpl(Request request) {
  pending_requests_.push_back(std::move(request));
  std::push_heap(
      pending_requests_.begin(), pending_requests_.end(), CompareRequests);
}

template <typename T>
bool RequestQueue<T>::empty() const {
  return pending_requests_.empty();
}

template <typename T>
size_t RequestQueue<T>::size() const {
  return pending_requests_.size();
}

template <typename T>
base::TimeTicks RequestQueue<T>::NextReleaseTime() const {
  return pending_requests_.front().backoff_entry->GetReleaseTime();
}

template <typename T>
void RequestQueue<T>::StartNextRequest() {
  if (active_request_) {
    // Already running a request, assume this method will be called again when
    // the request is done.
    return;
  }

  if (empty()) {
    // No requests in the queue, so we're done.
    return;
  }

  base::TimeTicks next_release = NextReleaseTime();
  base::TimeTicks now = base::TimeTicks::Now();
  if (next_release > now) {
    // Not ready for the next update check yet, call this method when it is
    // time.
    timer_.Start(FROM_HERE, next_release - now,
                 base::BindOnce(&RequestQueue<T>::StartNextRequest,
                                base::Unretained(this)));
    return;
  }

  // pop_heap swaps the first and last elements of pending_requests_, and after
  // that assures that the rest of pending_requests_ (excluding the
  // now last/formerly first element) forms a proper heap. After pop_heap
  // [begin, end-1) is a valid heap, and *(end - 1) contains the element that
  // used to be at the top of the heap. Since no elements are actually
  // removed from the container it is safe to read the entry being removed after
  // pop_heap is called (but before pop_back is called).
  std::pop_heap(
      pending_requests_.begin(), pending_requests_.end(), CompareRequests);

  active_request_ = std::move(pending_requests_.back());

  pending_requests_.pop_back();

  start_request_callback_.Run();
}

template <typename T>
void RequestQueue<T>::RetryRequest(const base::TimeDelta& min_backoff_delay) {
  DCHECK(active_request_);
  ScheduleRetriedRequest(reset_active_request(), min_backoff_delay);
}

template <typename T>
typename RequestQueue<T>::iterator RequestQueue<T>::begin() {
  return iterator(pending_requests_.begin());
}

template <typename T>
typename RequestQueue<T>::iterator RequestQueue<T>::end() {
  return iterator(pending_requests_.end());
}

template <typename T>
std::vector<std::unique_ptr<T>> RequestQueue<T>::erase_if(
    base::RepeatingCallback<bool(const T&)> condition) {
  std::vector<std::unique_ptr<T>> erased_fetches;
  for (size_t i = 0; i < pending_requests_.size();) {
    if (condition.Run(*pending_requests_[i].fetch)) {
      erased_fetches.emplace_back(std::move(pending_requests_[i].fetch));
      std::swap(pending_requests_[i],
                pending_requests_[pending_requests_.size() - 1]);
      pending_requests_.pop_back();
    } else {
      i++;
    }
  }
  // We need to maintain a heap structure on pending request in order to extract
  // first ones, but removing might break this structure.
  std::make_heap(pending_requests_.begin(), pending_requests_.end(),
                 CompareRequests);

  return erased_fetches;
}

template <typename T>
void RequestQueue<T>::set_backoff_policy(
    const net::BackoffEntry::Policy backoff_policy) {
  backoff_policy_ = backoff_policy;
}

// static
template <typename T>
bool RequestQueue<T>::CompareRequests(const Request& a, const Request& b) {
  return a.backoff_entry->GetReleaseTime() > b.backoff_entry->GetReleaseTime();
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_REQUEST_QUEUE_IMPL_H_
