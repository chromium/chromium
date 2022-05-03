// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_UNIQUE_PTR_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_UNIQUE_PTR_H_

#include <memory>

// As of Xcode 13.3, any C++ classes that contain a std::unique_ptr<T>
// fails to emit the definition to Swift, and use of the object in Swift
// results in an error: "cannot find 'T' in scope".
// https://github.com/apple/swift/issues/58639
#define SWIFT_INTEROP_UNIQUE_PTR_WORKS 0

class Value {
 public:
  Value(int value);
  ~Value();

  bool IsValid();
  int GetValue();

 private:
  int value_;
};

class ValueReturner {
 public:
  ValueReturner();
  ~ValueReturner();

  Value* ObjectPointer();

 private:
#if SWIFT_INTEROP_UNIQUE_PTR_WORKS
  std::unique_ptr<Value> object_;
#else
  Value* object_;
#endif
};

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_POINTER_UNIQUE_PTR_H_
