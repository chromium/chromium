// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/win/lib/lib_mutex.h"

#include <windows.h>

#include "base/win/windows_version.h"

namespace {

const long kTimeoutMs = 5000L;
const wchar_t kMutexName[] = L"{A946A6A9-917E-4949-B9BC-6BADA8C7FD63}";

}  // namespace

namespace rlz_lib {

LibMutex::LibMutex() : acquired_(false), mutex_(NULL) {
  mutex_ = CreateMutex(NULL, FALSE, kMutexName);
  if (mutex_)
    acquired_ = (WAIT_OBJECT_0 == WaitForSingleObject(mutex_, kTimeoutMs));
}

LibMutex::~LibMutex() {
  if (acquired_)
    ReleaseMutex(mutex_);
  if (mutex_)
    CloseHandle(mutex_);
}

}  // namespace rlz_lib
