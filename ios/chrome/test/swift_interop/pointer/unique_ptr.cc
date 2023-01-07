// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/pointer/unique_ptr.h"

ValueReturner::ValueReturner() {}

ValueReturner::~ValueReturner() {
#if !SWIFT_INTEROP_UNIQUE_PTR_WORKS
  delete object_;
#endif
}

Value* ValueReturner::ObjectPointer() {
#if SWIFT_INTEROP_UNIQUE_PTR_WORKS
  if (!object_) {
    object_ = std::make_unique<Value>(17);
  }
  return object_.get();
#else
  if (!object_) {
    object_ = new Value(17);
  }
  return object_;
#endif
}

Value::Value(int value) : value_(value) {}

Value::~Value() {}

bool Value::IsValid() {
  return true;
}

int Value::GetValue() {
  return value_;
}
