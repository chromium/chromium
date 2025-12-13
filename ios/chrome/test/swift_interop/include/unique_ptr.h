// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_UNIQUE_PTR_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_UNIQUE_PTR_H_

#include <memory>

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
  std::unique_ptr<Value> Object();
};

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_UNIQUE_PTR_H_
