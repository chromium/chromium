// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_THROTTLE_MANAGER_ACCESS_H_
#define EXTENSIONS_RENDERER_EXTENSION_THROTTLE_MANAGER_ACCESS_H_

#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace extensions {

class ExtensionThrottleManager;

// A thread-safe helper to access an ExtensionThrottleManager.
//
// ExtensionURLLoaderThrottle instances can be used and destroyed on threads
// other than where ExtensionThrottleManager lives. They may outlive the
// ExtensionThrottleManager that created them. This class provides a safe way
// for throttles to access the manager, checking for its validity before
// access it.
class ExtensionThrottleManagerAccess
    : public base::RefCountedThreadSafe<ExtensionThrottleManagerAccess> {
 public:
  explicit ExtensionThrottleManagerAccess(ExtensionThrottleManager* manager);

  // Invoked when the manager is destroyed.
  void SetDestroyed();

  // Locks manager access and returns an AutoLock and a pointer. If the
  // manager is already gone, the return pointer is nullptr. Otherwise, hold
  // on to the AutoLock to keep the manager pointer valid.
  std::tuple<base::MovableAutoLock, ExtensionThrottleManager*> Get();

 private:
  friend class base::RefCountedThreadSafe<ExtensionThrottleManagerAccess>;
  ~ExtensionThrottleManagerAccess();

  base::Lock lock_;
  raw_ptr<ExtensionThrottleManager> manager_ = nullptr;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_THROTTLE_MANAGER_ACCESS_H_
