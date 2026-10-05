// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/process_lock_shim.h"

#include "url/gurl.h"

namespace content::rust::process_lock {

std::unique_ptr<ProcessLock> CreateInvalid() {
  return std::make_unique<ProcessLock>();
}

std::unique_ptr<ProcessLock> Clone(const ProcessLock& lock) {
  return std::make_unique<ProcessLock>(lock);
}

::rust::String ToString(const ProcessLock& lock) {
  return ::rust::String(lock.ToString());
}

std::unique_ptr<GURL> GetProcessLockURL(const ProcessLock& lock) {
  return std::make_unique<GURL>(lock.GetProcessLockURL());
}

bool HaveSameWebExposedIsolationInfo(const ProcessLock& a,
                                     const ProcessLock& b) {
  return a.GetWebExposedIsolationInfo() == b.GetWebExposedIsolationInfo();
}

void SetIsUsed(ProcessLock& lock) {
  lock.set_is_used();
}

}  // namespace content::rust::process_lock
