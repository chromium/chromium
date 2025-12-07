// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/include/shared_ptr.h"

void RetainSharedObject(SharedObject* obj) {
  if (obj) {
    obj->AddRef();
    g_live_object_count++;
  }
}

void ReleaseSharedObject(SharedObject* obj) {
  if (obj) {
    obj->Release();
    g_live_object_count--;
  }
}

int GetSharedObjectLiveCount() {
  return g_live_object_count;
}

void ResetSharedObjectLiveCount() {
  g_live_object_count = 0;
}

SharedObject::SharedObject(int value) : value_(value) {}

SharedObject::~SharedObject() {}

SharedObject* SharedObject::MakeForSwift(int value) {
  // Create a shared object wrapped with scoped_ref. The reference count is 1.
  scoped_refptr<SharedObject> ptr = base::MakeRefCounted<SharedObject>(value);
  g_live_object_count++;
  // Leak ptr. The existing ref will be adopted by swift.
  return ptr.release();
}

bool SharedObject::IsValid() {
  return true;
}

int SharedObject::GetValue() {
  return value_;
}
