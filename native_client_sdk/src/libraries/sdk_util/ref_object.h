// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_SDK_UTIL_REF_OBJECT_H_
#define LIBRARIES_SDK_UTIL_REF_OBJECT_H_

#include <stdlib.h>
#include "pthread.h"

#include "sdk_util/atomicops.h"

namespace sdk_util {

class ScopedRefBase;

/*
 * RefObject
 *
 * A reference counted object. RefObjects should only be handled by ScopedRef
 * objects.
 *
 * When the object is first created, it has a reference count of zero.  It's
 * first incremented when it gets assigned to a ScopedRef.  When the last
 * ScopedRef is reset or destroyed the object will get released.
 */

class RefObject {
 public:
  RefObject() {
    ref_count_ = 0;
  }

  /*
   * RefCount
   *
   * RefCount returns an instantaneous snapshot of the RefCount, which may
   * change immediately after it is fetched.  This is only stable when all
   * pointers to the object are scoped pointers (ScopedRef), and the value
   * is one implying the current thread is the only one, with visibility to
   * the object.
   */
  int RefCount() const { return ref_count_; }

 protected:
  virtual ~RefObject() {}

  // Override to clean up object when last reference is released.
  virtual void Destroy() {}

  void Acquire() {
    AtomicAddFetch(&ref_count_, 1);
  }

  bool Release() {
    Atomic32 val = AtomicAddFetch(&ref_count_, -1);
    if (val == 0) {
      Destroy();
      delete this;
      return false;
    }
    return true;
  }

 private:
  Atomic32 ref_count_;

  friend class ScopedRefBase;
};

}  // namespace sdk_util

#endif  // LIBRARIES_SDK_UTIL_REF_OBJECT_H_

