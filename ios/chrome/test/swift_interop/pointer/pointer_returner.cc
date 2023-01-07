// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/pointer/pointer_returner.h"

PointerReturner::PointerReturner() : integer_(17), child_(nullptr) {}

PointerReturner::~PointerReturner() {
  if (child_)
    delete child_;
}

bool PointerReturner::Valid() {
  return true;
}

int* PointerReturner::IntegerPointer() {
  return &integer_;
}

PointerReturner* PointerReturner::ObjectPointer() {
  if (!child_) {
    child_ = new PointerReturner();
  }
  return child_;
}
