// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_request_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequest implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequest::~WebDataRequest() {
  WebDataRequestManager* manager = GetManager();
  if (manager) {
    manager->CancelRequest(handle_);
  }
}

WebDataServiceBase::Handle WebDataRequest::GetHandle() const {
  return handle_;
}

bool WebDataRequest::IsActive() {
  return GetManager() != nullptr;
}

WebDataRequest::WebDataRequest(WebDataRequestManager* manager,
                               WebDataServiceRequestCallback consumer,
                               WebDataServiceBase::Handle handle)
    : task_runner_(base::SequencedTaskRunner::HasCurrentDefault()
                       ? base::SequencedTaskRunner::GetCurrentDefault()
                       : nullptr),
      atomic_manager_(manager),
      consumer_(std::move(consumer)),
      handle_(handle) {
  DCHECK(IsActive());
  static_assert(sizeof(atomic_manager_) == sizeof(manager), "size mismatch");
}

WebDataRequestManager* WebDataRequest::GetManager() {
  return atomic_manager_.load(std::memory_order_acquire);
}

WebDataServiceRequestCallback WebDataRequest::ExtractConsumer() && {
  return std::move(consumer_);
}

scoped_refptr<base::SequencedTaskRunner> WebDataRequest::GetTaskRunner() {
  return task_runner_;
}

void WebDataRequest::MarkAsInactive() {
  atomic_manager_.store(nullptr, std::memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequestManager implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequestManager::WebDataRequestManager() : next_request_handle_(1) {}

std::unique_ptr<WebDataRequest> WebDataRequestManager::NewRequest(
    WebDataServiceRequestCallback consumer) {
  base::AutoLock l(pending_lock_);
  std::unique_ptr<WebDataRequest> request = base::WrapUnique(
      new WebDataRequest(this, std::move(consumer), next_request_handle_));
  bool inserted =
      pending_requests_.emplace(next_request_handle_, request.get()).second;
  DCHECK(inserted);
  ++next_request_handle_;
  return request;
}

void WebDataRequestManager::CancelRequest(WebDataServiceBase::Handle h) {
  base::AutoLock l(pending_lock_);
  // If the request was already cancelled, or has already completed, it won't
  // be in the pending_requests_ collection any more.
  auto i = pending_requests_.find(h);
  if (i == pending_requests_.end()) {
    return;
  }
  i->second->MarkAsInactive();
  pending_requests_.erase(i);
}

void WebDataRequestManager::RequestCompleted(
    std::unique_ptr<WebDataRequest> request,
    std::unique_ptr<WDTypedResult> result) {
  // Careful: Don't swap this below the BindOnce() call below, since that
  // effectively does a std::move() on |request|!
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      request->GetTaskRunner();
  auto task = base::BindOnce(&WebDataRequestManager::RequestCompletedOnThread,
                             this, std::move(request), std::move(result));
  if (task_runner) {
    task_runner->PostTask(FROM_HERE, std::move(task));
  } else {
    base::ThreadPool::PostTask(FROM_HERE, std::move(task));
  }
}

WebDataRequestManager::~WebDataRequestManager() {
  base::AutoLock l(pending_lock_);
  for (auto& pending_request : pending_requests_) {
    pending_request.second->MarkAsInactive();
  }
  pending_requests_.clear();
}

void WebDataRequestManager::RequestCompletedOnThread(
    std::unique_ptr<WebDataRequest> request,
    std::unique_ptr<WDTypedResult> result) {
  // Check whether the request is active. It might have been cancelled in
  // another thread before this completion handler was invoked. This means the
  // request initiator is no longer interested in the result.
  if (!request->IsActive()) {
    return;
  }

  WebDataServiceBase::Handle handle = request->GetHandle();
  WebDataServiceRequestCallback consumer =
      std::move(*request).ExtractConsumer();

  // Stop tracking the request. The request is already finished, so "stop
  // tracking" is the same as post-facto cancellation.
  CancelRequest(handle);

  // Notify the consumer if needed.
  if (!consumer.is_null()) {
    std::move(consumer).Run(handle, std::move(result));
  }
}
