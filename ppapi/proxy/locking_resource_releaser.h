// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_LOCKING_RESOURCE_RELEASER_H_
#define PPAPI_PROXY_LOCKING_RESOURCE_RELEASER_H_

#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace ppapi {
namespace proxy {

// LockingResourceReleaser is a simple RAII class for releasing a resource at
// the end of scope. This acquires the ProxyLock before releasing the resource.
// It is for use in unit tests. Most proxy or implementation code should use
// ScopedPPResource instead. Unit tests sometimes can't use ScopedPPResource
// because it asserts that the ProxyLock is already held.
class LockingResourceReleaser {
 public:
  explicit LockingResourceReleaser(PP_Resource resource)
      : resource_(resource) {
  }

  LockingResourceReleaser(const LockingResourceReleaser&) = delete;
  LockingResourceReleaser& operator=(const LockingResourceReleaser&) = delete;

  ~LockingResourceReleaser() {
    ProxyAutoLock lock;
    PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(resource_);
  }

  PP_Resource get() { return resource_; }

 private:
  PP_Resource resource_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_LOCKING_RESOURCE_RELEASER_H_
