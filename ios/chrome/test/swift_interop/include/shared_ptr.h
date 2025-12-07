// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_SHARED_PTR_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_SHARED_PTR_H_

#include "base/apple/swift_interop_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "ios/chrome/test/swift_interop/include/unique_ptr.h"

static int g_live_object_count = 0;

class SharedObject;

void RetainSharedObject(SharedObject* obj);
void ReleaseSharedObject(SharedObject* obj);

int GetSharedObjectLiveCount();
void ResetSharedObjectLiveCount();

class SharedObject : public base::RefCounted<SharedObject> {
 public:
  SharedObject(const SharedObject&) = delete;  // non-copyable
  explicit SharedObject(int value);
  static SharedObject* MakeForSwift(int value) SWIFT_RETURNS_RETAINED;

  bool IsValid();
  int GetValue();

 private:
  friend class base::RefCounted<SharedObject>;
  ~SharedObject();

  int value_;
} SWIFT_SHARED_REFERENCE(RetainSharedObject, ReleaseSharedObject);

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_SHARED_PTR_H_
