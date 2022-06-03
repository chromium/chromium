// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_REQUEST_QUEUE_IMPL_H_
#define EXTENSIONS_BROWSER_UPDATER_REQUEST_QUEUE_IMPL_H_

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "extensions/browser/updater/request_queue.h"

namespace extensions {

template <typename T>
RequestQueue<T>::RequestQueue(
    const net::BackoffEntry::Policy* const backoff_policy,
    const base::RepeatingClosure& start_request_callback)
    : backoff_policy_(backoff_policy),
      start_request_callback_(start_request_callback) {}

template <typename T>
RequestQueue<T>::~RequestQueue() = default;

template <typename T>
T* RequestQueue<T>::active_request() {
  return active_request_.get();
}

template <typename T>
int RequestQueue<T>::active_request_failure_count() {
  return active_backoff_entry_->failure_count();
}

template <typename T>
std::unique_ptr<T> RequestQueue<T>::reset_active_request() {
  active_backoff_entry_.reset();
  return std::move(active_request_);
}

template <typename T>
void RequestQueue<T>::ScheduleRequest(std::unique_ptr<T> request) {
  PushImpl(std::move(request), std::unique_ptr<net::BackoffEntry>(
                                   new net::BackoffEntry(backoff_policy_)));
  StartNextRequest();
}

template <typename T>
void RequestQueue<T>::PushImpl(
    std::unique_ptr<T> request,
    std::unique_ptr<net::BackoffEntry> backoff_entry) {
  pending_requests_.push_back(
      Request(backoff_entry.release(), request.release()));
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
  if (active_request_)
    // Already running a request, assume this method will be called again when
    // the request is done.
    return;

  if (empty())
    // No requests in the queue, so we're done.
    return;

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

  active_backoff_entry_ = std::move(pending_requests_.back().backoff_entry);
  active_request_ = std::move(pending_requests_.back().request);

  pending_requests_.pop_back();

  start_request_callback_.Run();
}

template <typename T>
void RequestQueue<T>::RetryRequest(const base::TimeDelta& min_backoff_delay) {
  active_backoff_entry_->InformOfRequest(false);
  if (active_backoff_entry_->GetTimeUntilRelease() < min_backoff_delay) {
    active_backoff_entry_->SetCustomReleaseTime(base::TimeTicks::Now() +
                                                min_backoff_delay);
  }
  PushImpl(std::move(active_request_), std::move(active_backoff_entry_));
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
void RequestQueue<T>::set_backoff_policy(
    const net::BackoffEntry::Policy* backoff_policy) {
  backoff_policy_ = backoff_policy;
}

// static
template <typename T>
bool RequestQueue<T>::CompareRequests(const Request& a, const Request& b) {
  return a.backoff_entry->GetReleaseTime() > b.backoff_entry->GetReleaseTime();
}

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_REQUEST_QUEUE_IMPL_H_
