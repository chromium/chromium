// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_CLOSURE_OBSERVER_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_CLOSURE_OBSERVER_H_

#include <functional>

#include "base/apple/swift_interop_util.h"

class ValueObserver;

void Retain_ValueObserver(ValueObserver* observer);
void Release_ValueObserver(ValueObserver* observer);

class ValueObserver {
 public:
  using ValueDidChangeCallback = std::function<void(int)>;

  ValueObserver() = default;
  virtual ~ValueObserver() = default;

  static ValueObserver* MakeForSwift() SWIFT_RETURNS_RETAINED;

  virtual ValueDidChangeCallback GetValueDidChangeCallback() = 0;
  virtual void ValueDidChange(int value) = 0;
  virtual int value() = 0;
} SWIFT_SHARED_REFERENCE(Retain_ValueObserver, Release_ValueObserver);

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_CLOSURE_OBSERVER_H_
