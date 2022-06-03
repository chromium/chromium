// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/proxy_lock.h"

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "ppapi/shared_impl/ppapi_globals.h"

namespace ppapi {

base::LazyInstance<base::Lock>::Leaky g_proxy_lock = LAZY_INSTANCE_INITIALIZER;

bool g_disable_locking = false;
base::LazyInstance<base::ThreadLocalBoolean>::Leaky
    g_disable_locking_for_thread = LAZY_INSTANCE_INITIALIZER;

// Simple single-thread deadlock detector for the proxy lock.
// |true| when the current thread has the lock.
base::LazyInstance<base::ThreadLocalBoolean>::Leaky g_proxy_locked_on_thread =
    LAZY_INSTANCE_INITIALIZER;

// static
base::Lock* ProxyLock::Get() {
  if (g_disable_locking || g_disable_locking_for_thread.Get().Get())
    return NULL;
  return &g_proxy_lock.Get();
}

// Functions below should only access the lock via Get to ensure that they don't
// try to use the lock on the host side of the proxy, where locking is
// unnecessary and wrong (because we haven't coded the host side to account for
// locking).

// static
void ProxyLock::Acquire() NO_THREAD_SAFETY_ANALYSIS {
  // NO_THREAD_SAFETY_ANALYSIS: Runtime dependent locking.
  base::Lock* lock = Get();
  if (lock) {
    // This thread does not already hold the lock.
    const bool deadlock = g_proxy_locked_on_thread.Get().Get();
    CHECK(!deadlock);

    lock->Acquire();
    g_proxy_locked_on_thread.Get().Set(true);
  }
}

// static
void ProxyLock::Release() NO_THREAD_SAFETY_ANALYSIS {
  // NO_THREAD_SAFETY_ANALYSIS: Runtime dependent locking.
  base::Lock* lock = Get();
  if (lock) {
    // This thread currently holds the lock.
    const bool locked = g_proxy_locked_on_thread.Get().Get();
    CHECK(locked);

    g_proxy_locked_on_thread.Get().Set(false);
    lock->Release();
  }
}

// static
void ProxyLock::AssertAcquired() {
  base::Lock* lock = Get();
  if (lock) {
    // This thread currently holds the lock.
    const bool locked = g_proxy_locked_on_thread.Get().Get();
    CHECK(locked);

    lock->AssertAcquired();
  }
}

// static
void ProxyLock::DisableLocking() {
  // Note, we don't DCHECK that this flag isn't already set, because multiple
  // unit tests may run in succession and all set it.
  g_disable_locking = true;
}

ProxyLock::LockingDisablerForTest::LockingDisablerForTest() {
  // Note, we don't DCHECK that this flag isn't already set, because multiple
  // unit tests may run in succession and all set it.
  g_disable_locking_for_thread.Get().Set(true);
}

ProxyLock::LockingDisablerForTest::~LockingDisablerForTest() {
  g_disable_locking_for_thread.Get().Set(false);
}

void CallWhileUnlocked(base::OnceClosure closure) {
  ProxyAutoUnlock lock;
  std::move(closure).Run();
}

}  // namespace ppapi
