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
  if (manager)
    manager->CancelRequest(handle_);
}

WebDataServiceBase::Handle WebDataRequest::GetHandle() const {
  return handle_;
}

bool WebDataRequest::IsActive() {
  return GetManager() != nullptr;
}

WebDataRequest::WebDataRequest(WebDataRequestManager* manager,
                               WebDataServiceConsumer* consumer,
                               WebDataServiceBase::Handle handle)
    : task_runner_(base::SequencedTaskRunner::HasCurrentDefault()
                       ? base::SequencedTaskRunner::GetCurrentDefault()
                       : nullptr),
      atomic_manager_(reinterpret_cast<base::subtle::AtomicWord>(manager)),
      consumer_(consumer ? consumer->GetWebDataServiceConsumerWeakPtr()
                         : nullptr),
      handle_(handle) {
  DCHECK(IsActive());
  static_assert(sizeof(atomic_manager_) == sizeof(manager), "size mismatch");
}

WebDataRequestManager* WebDataRequest::GetManager() {
  return reinterpret_cast<WebDataRequestManager*>(
      base::subtle::Acquire_Load(&atomic_manager_));
}

WebDataServiceConsumer* WebDataRequest::GetConsumer() {
  return consumer_.get();
}

scoped_refptr<base::SequencedTaskRunner> WebDataRequest::GetTaskRunner() {
  return task_runner_;
}

void WebDataRequest::MarkAsInactive() {
  // Set atomic_manager_ to the equivalent of nullptr;
  base::subtle::Release_Store(&atomic_manager_, 0);
}

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequestManager implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequestManager::WebDataRequestManager()
    : next_request_handle_(1) {
}

std::unique_ptr<WebDataRequest> WebDataRequestManager::NewRequest(
    WebDataServiceConsumer* consumer) {
  base::AutoLock l(pending_lock_);
  std::unique_ptr<WebDataRequest> request = base::WrapUnique(
      new WebDataRequest(this, consumer, next_request_handle_));
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
  if (i == pending_requests_.end())
    return;
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
  if (task_runner)
    task_runner->PostTask(FROM_HERE, std::move(task));
  else
    base::ThreadPool::PostTask(FROM_HERE, std::move(task));
}

WebDataRequestManager::~WebDataRequestManager() {
  base::AutoLock l(pending_lock_);
  for (auto i = pending_requests_.begin(); i != pending_requests_.end(); ++i)
    i->second->MarkAsInactive();
  pending_requests_.clear();
}

void WebDataRequestManager::RequestCompletedOnThread(
    std::unique_ptr<WebDataRequest> request,
    std::unique_ptr<WDTypedResult> result) {
  // Check whether the request is active. It might have been cancelled in
  // another thread before this completion handler was invoked. This means the
  // request initiator is no longer interested in the result.
  if (!request->IsActive())
    return;

  // Stop tracking the request. The request is already finished, so "stop
  // tracking" is the same as post-facto cancellation.
  CancelRequest(request->GetHandle());

  // Notify the consumer if needed.
  WebDataServiceConsumer* const consumer = request->GetConsumer();
  if (consumer) {
    consumer->OnWebDataServiceRequestDone(request->GetHandle(),
                                          std::move(result));
  }
}
