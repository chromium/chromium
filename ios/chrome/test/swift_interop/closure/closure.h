// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_CLOSURE_CLOSURE_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_CLOSURE_CLOSURE_H_

#include <functional>

#include "base/apple/swift_interop_util.h"

class ClosureProvider;

void Retain_ClosureProvider(ClosureProvider* provider);
void Release_ClosureProvider(ClosureProvider* provider);

class ClosureProvider {
 public:
  ClosureProvider() = default;
  virtual ~ClosureProvider() = default;

  static ClosureProvider* MakeForSwift() SWIFT_RETURNS_RETAINED;

  virtual std::function<void()> GetRepeatingClosure() = 0;
  virtual std::function<void()> GetOnceClosure() = 0;
  virtual int call_count() = 0;
} SWIFT_SHARED_REFERENCE(Retain_ClosureProvider, Release_ClosureProvider);

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_CLOSURE_CLOSURE_H_
