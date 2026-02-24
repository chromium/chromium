// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_SHARED_PTR_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_SHARED_PTR_H_

#include "base/apple/swift_interop_util.h"

class SharedObject;

void Retain_SharedObject(SharedObject* obj);
void Release_SharedObject(SharedObject* obj);

int GetTotalReferenceCount();
void ResetTotalReferenceCount();

class SharedObject {
 public:
  SharedObject(const SharedObject&) = delete;  // non-copyable

  SharedObject() = default;
  virtual ~SharedObject() = default;

  static SharedObject* MakeForSwift(int value) SWIFT_RETURNS_RETAINED;

  virtual bool IsValid() const = 0;
  virtual int GetValue() const = 0;
} SWIFT_SHARED_REFERENCE(Retain_SharedObject, Release_SharedObject);

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_SHARED_PTR_H_
