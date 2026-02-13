// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_endpoint_lock_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// This delay prevents DoS attacks.
// TODO(ricea): Replace this with randomised truncated exponential backoff.
// See crbug.com/377613.
const int kUnlockDelayInMs = 10;

}  // namespace

WebSocketEndpointLockManager::EndpointLock::EndpointLock(
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
    const IPEndPoint& endpoint)
    : websocket_endpoint_lock_manager_(websocket_endpoint_lock_manager),
      endpoint_(endpoint) {}

WebSocketEndpointLockManager::EndpointLock::~EndpointLock() {
  if (next()) {
    // If in queue, remove `this`.
    DCHECK(previous());
    RemoveFromList();
  }
  // Release lock if held.
  if (lock_info_) {
    websocket_endpoint_lock_manager_->UnlockEndpointInternal(endpoint_,
                                                             *lock_info_);
  }
}

int WebSocketEndpointLockManager::EndpointLock::LockEndpoint(
    base::OnceClosure lock_callback) {
  DCHECK(!lock_callback_);
  DCHECK(lock_callback);

  int result = websocket_endpoint_lock_manager_->LockEndpoint(endpoint_, this);
  if (result == ERR_IO_PENDING) {
    lock_callback_ = std::move(lock_callback);
  }
  return result;
}

void WebSocketEndpointLockManager::EndpointLock::GotEndpointLock() {
  DCHECK(lock_callback_);

  std::move(lock_callback_).Run();
}

WebSocketEndpointLockManager::WebSocketEndpointLockManager()
    : unlock_delay_(base::Milliseconds(kUnlockDelayInMs)) {}

WebSocketEndpointLockManager::~WebSocketEndpointLockManager() {
  DCHECK_EQ(lock_info_map_.size(), pending_unlock_count_);
}

void WebSocketEndpointLockManager::UnlockEndpoint(const IPEndPoint& endpoint) {
  auto lock_info_it = lock_info_map_.find(endpoint);
  // Nothing to do if the lock is not held. This is not an error.
  if (lock_info_it == lock_info_map_.end()) {
    return;
  }

  UnlockEndpointInternal(endpoint, lock_info_it->second);
}

bool WebSocketEndpointLockManager::IsEmpty() const {
  return lock_info_map_.empty();
}

base::TimeDelta WebSocketEndpointLockManager::SetUnlockDelayForTesting(
    base::TimeDelta new_delay) {
  base::TimeDelta old_delay = unlock_delay_;
  unlock_delay_ = new_delay;
  return old_delay;
}

WebSocketEndpointLockManager::LockInfo::LockInfo() = default;
WebSocketEndpointLockManager::LockInfo::~LockInfo() {
  DCHECK(!endpoint_lock);
}

WebSocketEndpointLockManager::LockInfo::LockInfo(const LockInfo& rhs)
    : endpoint_lock(rhs.endpoint_lock) {
  DCHECK(!rhs.queue);
}

int WebSocketEndpointLockManager::LockEndpoint(const IPEndPoint& endpoint,
                                               EndpointLock* endpoint_lock) {
  LockInfoMap::value_type insert_value(endpoint, LockInfo());
  std::pair<LockInfoMap::iterator, bool> rv =
      lock_info_map_.insert(insert_value);
  LockInfo& lock_info_in_map = rv.first->second;
  if (rv.second) {
    DVLOG(3) << "Locking endpoint " << endpoint.ToString();
    lock_info_in_map.queue = std::make_unique<LockInfo::WaiterQueue>();
    // The endpoint is now locked by `endpoint_lock`.
    SetLock(&lock_info_in_map, endpoint_lock);
    return OK;
  }
  DVLOG(3) << "Waiting for endpoint " << endpoint.ToString();
  lock_info_in_map.queue->Append(endpoint_lock);
  return ERR_IO_PENDING;
}

void WebSocketEndpointLockManager::UnlockEndpointInternal(
    const IPEndPoint& endpoint,
    LockInfo& lock_info) {
  ClearLock(lock_info);
  UnlockEndpointAfterDelay(endpoint);
}

void WebSocketEndpointLockManager::UnlockEndpointAfterDelay(
    const IPEndPoint& endpoint) {
  DVLOG(3) << "Delaying " << unlock_delay_.InMilliseconds()
           << "ms before unlocking endpoint " << endpoint.ToString();
  ++pending_unlock_count_;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebSocketEndpointLockManager::DelayedUnlockEndpoint,
                     weak_factory_.GetWeakPtr(), endpoint),
      unlock_delay_);
}

void WebSocketEndpointLockManager::DelayedUnlockEndpoint(
    const IPEndPoint& endpoint) {
  auto lock_info_it = lock_info_map_.find(endpoint);
  DCHECK_GT(pending_unlock_count_, 0U);
  --pending_unlock_count_;
  if (lock_info_it == lock_info_map_.end())
    return;
  DCHECK(!lock_info_it->second.endpoint_lock);
  LockInfo::WaiterQueue* queue = lock_info_it->second.queue.get();
  DCHECK(queue);
  if (queue->empty()) {
    DVLOG(3) << "Unlocking endpoint " << lock_info_it->first.ToString();
    lock_info_map_.erase(lock_info_it);
    return;
  }

  DVLOG(3) << "Unlocking endpoint " << lock_info_it->first.ToString()
           << " and activating next waiter";
  EndpointLock* endpoint_lock = queue->head()->value();
  // The endpoint is now locked by `endpoint_lock`.
  SetLock(&lock_info_it->second, endpoint_lock);
  endpoint_lock->RemoveFromList();
  endpoint_lock->GotEndpointLock();
}

void WebSocketEndpointLockManager::SetLock(LockInfo* lock_info,
                                           EndpointLock* endpoint_lock) {
  DCHECK(endpoint_lock);
  DCHECK(!lock_info->endpoint_lock);
  DCHECK(!endpoint_lock->lock_info_);

  lock_info->endpoint_lock = endpoint_lock;
  endpoint_lock->lock_info_ = lock_info;
}

void WebSocketEndpointLockManager::ClearLock(LockInfo& lock_info) {
  DCHECK(!lock_info.endpoint_lock ||
         &lock_info == lock_info.endpoint_lock->lock_info_);
  lock_info.endpoint_lock->lock_info_ = nullptr;
  lock_info.endpoint_lock = nullptr;
}

}  // namespace net
