// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/threadpool.h"

#include <stddef.h>

#include "sandbox/win/src/win_utils.h"

namespace sandbox {

ThreadPool::ThreadPool() {
  ::InitializeCriticalSection(&lock_);
}

bool ThreadPool::RegisterWait(const void* cookie,
                              HANDLE waitable_object,
                              CrossCallIPCCallback callback,
                              void* context) {
  if (0 == cookie) {
    return false;
  }
  HANDLE pool_object = nullptr;
  // create a wait for a kernel object, with no timeout
  if (!::RegisterWaitForSingleObject(&pool_object, waitable_object, callback,
                                     context, INFINITE, WT_EXECUTEDEFAULT)) {
    return false;
  }
  PoolObject pool_obj = {cookie, pool_object};
  AutoLock lock(&lock_);
  pool_objects_.push_back(pool_obj);
  return true;
}

bool ThreadPool::UnRegisterWaits(void* cookie) {
  if (0 == cookie) {
    return false;
  }
  std::vector<HANDLE> finished_waits;
  {
    AutoLock lock(&lock_);
    PoolObjects::iterator it = pool_objects_.begin();
    while (it != pool_objects_.end()) {
      if (it->cookie == cookie) {
        HANDLE wait = it->wait;
        it = pool_objects_.erase(it);
        finished_waits.push_back(wait);
      } else {
        ++it;
      }
    }
  }
  bool success = true;
  for (HANDLE wait : finished_waits) {
    success &= (::UnregisterWaitEx(wait, INVALID_HANDLE_VALUE) != 0);
  }
  return success;
}

size_t ThreadPool::OutstandingWaits() {
  AutoLock lock(&lock_);
  return pool_objects_.size();
}

ThreadPool::~ThreadPool() {
  // Here we used to unregister all the pool wait handles. Now, following the
  // rest of the code we avoid lengthy or blocking calls given that the process
  // is being torn down.
  ::DeleteCriticalSection(&lock_);
}

}  // namespace sandbox
