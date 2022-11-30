// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/utility/threading/lock.h"

namespace pp {

#ifdef WIN32  // Windows implementation for native plugins.

Lock::Lock() {
  // The second parameter is the spin count; for short-held locks it avoids the
  // contending thread from going to sleep which helps performance greatly.
  ::InitializeCriticalSectionAndSpinCount(&os_lock_, 2000);
}

Lock::~Lock() {
  ::DeleteCriticalSection(&os_lock_);
}

void Lock::Acquire() {
  ::EnterCriticalSection(&os_lock_);
}

void Lock::Release() {
  ::LeaveCriticalSection(&os_lock_);
}

#else  // Posix implementation.

Lock::Lock() {
  pthread_mutex_init(&os_lock_, NULL);
}

Lock::~Lock() {
  pthread_mutex_destroy(&os_lock_);
}

void Lock::Acquire() {
  pthread_mutex_lock(&os_lock_);
}

void Lock::Release() {
  pthread_mutex_unlock(&os_lock_);
}

#endif

}  // namespace pp
