// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/include/unique_ptr.h"

std::unique_ptr<Value> ValueReturner::Object() {
  return std::make_unique<Value>(42);
}

Value::Value(int value) : value_(value) {}

Value::~Value() {}

bool Value::IsValid() {
  return true;
}

int Value::GetValue() {
  return value_;
}
