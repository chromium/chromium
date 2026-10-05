// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROCESS_LOCK_SHIM_H_
#define CONTENT_BROWSER_PROCESS_LOCK_SHIM_H_

#include <memory>
#include <string>

#include "content/browser/process_lock.h"
#include "third_party/rust/cxx/v1/cxx.h"

class GURL;

namespace content::rust::process_lock {

// Shim functions for Rust FFI bindings to interact with ProcessLock.

// Creates a default-constructed ProcessLock in the "invalid" state (where
// is_invalid() is true). This represents a newly created process that has not
// yet been associated with any SiteInstance and should not be granted access
// to any site data.
std::unique_ptr<ProcessLock> CreateInvalid();

// Returns a heap-allocated copy of `lock`, allowing Rust code holding a
// UniquePtr<ProcessLock> to duplicate it across FFI.
std::unique_ptr<ProcessLock> Clone(const ProcessLock& lock);

// Returns a Rust string representation of `lock` via ProcessLock::ToString().
::rust::String ToString(const ProcessLock& lock);

// Returns a heap-allocated GURL representing the lock's site/origin URL
// (wrapping ProcessLock::GetProcessLockURL(), which returns by value).
std::unique_ptr<GURL> GetProcessLockURL(const ProcessLock& lock);

// Returns whether `a` and `b` share the same WebExposedIsolationInfo.
bool HaveSameWebExposedIsolationInfo(const ProcessLock& a,
                                     const ProcessLock& b);

// Marks the process associated with `lock` as used. Unlike the other shims
// here, this is also called directly from the C++
// ChildProcessSecurityPolicyImpl (not only from Rust). It is the only function
// allowed to call the private ProcessLock::set_is_used().
void SetIsUsed(ProcessLock& lock);

}  // namespace content::rust::process_lock

#endif  // CONTENT_BROWSER_PROCESS_LOCK_SHIM_H_
