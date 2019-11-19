// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_endpoint_lock_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

// This delay prevents DoS attacks.
// TODO(ricea): Replace this with randomised truncated exponential backoff.
// See crbug.com/377613.
const int kUnlockDelayInMs = 10;

}  // namespace

WebSocketEndpointLockManager::Waiter::~Waiter() {
  if (next()) {
    DCHECK(previous());
    RemoveFromList();
  }
}

WebSocketEndpointLockManager::LockReleaser::LockReleaser(
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
    IPEndPoint endpoint)
    : websocket_endpoint_lock_manager_(websocket_endpoint_lock_manager),
      endpoint_(endpoint) {
  websocket_endpoint_lock_manager->RegisterLockReleaser(this, endpoint);
}

WebSocketEndpointLockManager::LockReleaser::~LockReleaser() {
  if (websocket_endpoint_lock_manager_) {
    websocket_endpoint_lock_manager_->UnlockEndpoint(endpoint_);
  }
}

WebSocketEndpointLockManager::WebSocketEndpointLockManager()
    : unlock_delay_(base::TimeDelta::FromMilliseconds(kUnlockDelayInMs)),
      pending_unlock_count_(0) {}

WebSocketEndpointLockManager::~WebSocketEndpointLockManager() {
  DCHECK_EQ(lock_info_map_.size(), pending_unlock_count_);
}

int WebSocketEndpointLockManager::LockEndpoint(const IPEndPoint& endpoint,
                                               Waiter* waiter) {
  LockInfoMap::value_type insert_value(endpoint, LockInfo());
  std::pair<LockInfoMap::iterator, bool> rv =
      lock_info_map_.insert(insert_value);
  LockInfo& lock_info_in_map = rv.first->second;
  if (rv.second) {
    DVLOG(3) << "Locking endpoint " << endpoint.ToString();
    lock_info_in_map.queue.reset(new LockInfo::WaiterQueue);
    return OK;
  }
  DVLOG(3) << "Waiting for endpoint " << endpoint.ToString();
  lock_info_in_map.queue->Append(waiter);
  return ERR_IO_PENDING;
}

void WebSocketEndpointLockManager::UnlockEndpoint(const IPEndPoint& endpoint) {
  auto lock_info_it = lock_info_map_.find(endpoint);
  if (lock_info_it == lock_info_map_.end())
    return;
  LockReleaser* lock_releaser = lock_info_it->second.lock_releaser;
  if (lock_releaser) {
    lock_info_it->second.lock_releaser = nullptr;
    lock_releaser->websocket_endpoint_lock_manager_ = nullptr;
  }
  UnlockEndpointAfterDelay(endpoint);
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

WebSocketEndpointLockManager::LockInfo::LockInfo() : lock_releaser(nullptr) {}
WebSocketEndpointLockManager::LockInfo::~LockInfo() {
  DCHECK(!lock_releaser);
}

WebSocketEndpointLockManager::LockInfo::LockInfo(const LockInfo& rhs)
    : lock_releaser(rhs.lock_releaser) {
  DCHECK(!rhs.queue);
}

void WebSocketEndpointLockManager::RegisterLockReleaser(
    LockReleaser* lock_releaser,
    IPEndPoint endpoint) {
  DCHECK(lock_releaser);
  auto lock_info_it = lock_info_map_.find(endpoint);
  CHECK(lock_info_it != lock_info_map_.end());
  DCHECK(!lock_info_it->second.lock_releaser);
  lock_info_it->second.lock_releaser = lock_releaser;
  DVLOG(3) << "Registered (LockReleaser*)" << lock_releaser << " for "
           << endpoint.ToString();
}

void WebSocketEndpointLockManager::UnlockEndpointAfterDelay(
    const IPEndPoint& endpoint) {
  DVLOG(3) << "Delaying " << unlock_delay_.InMilliseconds()
           << "ms before unlocking endpoint " << endpoint.ToString();
  ++pending_unlock_count_;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
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
  DCHECK(!lock_info_it->second.lock_releaser);
  LockInfo::WaiterQueue* queue = lock_info_it->second.queue.get();
  DCHECK(queue);
  if (queue->empty()) {
    DVLOG(3) << "Unlocking endpoint " << lock_info_it->first.ToString();
    lock_info_map_.erase(lock_info_it);
    return;
  }

  DVLOG(3) << "Unlocking endpoint " << lock_info_it->first.ToString()
           << " and activating next waiter";
  Waiter* next_job = queue->head()->value();
  next_job->RemoveFromList();
  next_job->GotEndpointLock();
}

}  // namespace net
