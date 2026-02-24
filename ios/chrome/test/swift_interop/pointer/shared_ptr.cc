// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/pointer/shared_ptr.h"

#include "base/memory/ref_counted.h"

class SharedObjectImpl : public SharedObject,
                         public base::RefCountedThreadSafe<SharedObjectImpl> {
 public:
  explicit SharedObjectImpl(int value) : SharedObject(), value_(value) {}

  bool IsValid() const override { return true; }
  int GetValue() const override { return value_; }

 private:
  friend class base::RefCountedThreadSafe<SharedObjectImpl>;
  ~SharedObjectImpl() override = default;

  int value_;
};

// Tracks the total number of active references (Retain calls - Release calls)
// across all SharedObject instances.
static int g_total_ref_count = 0;

int GetTotalReferenceCount() {
  return g_total_ref_count;
}

void ResetTotalReferenceCount() {
  g_total_ref_count = 0;
}

SharedObject* SharedObject::MakeForSwift(int value) {
  scoped_refptr<SharedObjectImpl> ptr =
      base::MakeRefCounted<SharedObjectImpl>(value);
  //  The SWIFT_RETURNS_RETAINED annotation requires that the returned value is
  //  passed with +1 ownership.
  Retain_SharedObject(ptr.get());
  // Leak ptr. The existing ref will be adopted by swift.
  return ptr.release();
}

void Retain_SharedObject(SharedObject* obj) {
  if (obj) {
    g_total_ref_count++;
    static_cast<SharedObjectImpl*>(obj)->AddRef();
  }
}

void Release_SharedObject(SharedObject* obj) {
  if (obj) {
    g_total_ref_count--;
    static_cast<SharedObjectImpl*>(obj)->Release();
  }
}
