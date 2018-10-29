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

WebSocketEndpointLockManager::WebSocketEndpointLockManager()
    : unlock_delay_(base::TimeDelta::FromMilliseconds(kUnlockDelayInMs)),
      pending_unlock_count_(0),
      weak_factory_(this) {}

WebSocketEndpointLockManager::~WebSocketEndpointLockManager() {
  DCHECK_EQ(lock_info_map_.size(), pending_unlock_count_);
  DCHECK(socket_lock_info_map_.empty());
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

void WebSocketEndpointLockManager::RememberSocket(StreamSocket* socket,
                                                  const IPEndPoint& endpoint) {
  auto lock_info_it = lock_info_map_.find(endpoint);
  CHECK(lock_info_it != lock_info_map_.end());
  bool inserted =
      socket_lock_info_map_.insert(SocketLockInfoMap::value_type(
                                       socket, lock_info_it)).second;
  DCHECK(inserted);
  DCHECK(!lock_info_it->second.socket);
  lock_info_it->second.socket = socket;
  DVLOG(3) << "Remembered (StreamSocket*)" << socket << " for "
           << endpoint.ToString() << " (" << socket_lock_info_map_.size()
           << " socket(s) remembered)";
}

void WebSocketEndpointLockManager::UnlockSocket(StreamSocket* socket) {
  auto socket_it = socket_lock_info_map_.find(socket);
  if (socket_it == socket_lock_info_map_.end())
    return;

  auto lock_info_it = socket_it->second;

  DVLOG(3) << "Unlocking (StreamSocket*)" << socket << " for "
           << lock_info_it->first.ToString() << " ("
           << socket_lock_info_map_.size() << " socket(s) left)";
  socket_lock_info_map_.erase(socket_it);
  DCHECK_EQ(socket, lock_info_it->second.socket);
  lock_info_it->second.socket = nullptr;
  UnlockEndpointAfterDelay(lock_info_it->first);
}

void WebSocketEndpointLockManager::UnlockEndpoint(const IPEndPoint& endpoint) {
  auto lock_info_it = lock_info_map_.find(endpoint);
  if (lock_info_it == lock_info_map_.end())
    return;
  if (lock_info_it->second.socket)
    EraseSocket(lock_info_it);
  UnlockEndpointAfterDelay(endpoint);
}

bool WebSocketEndpointLockManager::IsEmpty() const {
  return lock_info_map_.empty() && socket_lock_info_map_.empty();
}

base::TimeDelta WebSocketEndpointLockManager::SetUnlockDelayForTesting(
    base::TimeDelta new_delay) {
  base::TimeDelta old_delay = unlock_delay_;
  unlock_delay_ = new_delay;
  return old_delay;
}

WebSocketEndpointLockManager::LockInfo::LockInfo() : socket(nullptr) {}
WebSocketEndpointLockManager::LockInfo::~LockInfo() {
  DCHECK(!socket);
}

WebSocketEndpointLockManager::LockInfo::LockInfo(const LockInfo& rhs)
    : socket(rhs.socket) {
  DCHECK(!rhs.queue);
}

void WebSocketEndpointLockManager::UnlockEndpointAfterDelay(
    const IPEndPoint& endpoint) {
  DVLOG(3) << "Delaying " << unlock_delay_.InMilliseconds()
           << "ms before unlocking endpoint " << endpoint.ToString();
  ++pending_unlock_count_;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebSocketEndpointLockManager::DelayedUnlockEndpoint,
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
  DCHECK(!lock_info_it->second.socket);
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

void WebSocketEndpointLockManager::EraseSocket(
    LockInfoMap::iterator lock_info_it) {
  DVLOG(3) << "Removing (StreamSocket*)" << lock_info_it->second.socket
           << " for " << lock_info_it->first.ToString() << " ("
           << socket_lock_info_map_.size() << " socket(s) left)";
  size_t erased = socket_lock_info_map_.erase(lock_info_it->second.socket);
  DCHECK_EQ(1U, erased);
  lock_info_it->second.socket = nullptr;
}

}  // namespace net
