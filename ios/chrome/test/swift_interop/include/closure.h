// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_CLOSURE_H_
#define IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_CLOSURE_H_

#include "base/apple/swift_interop_util.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

SWIFT_DECLARE_INTEROP_WRAPPER(CxxRepeatingClosure, base::RepeatingClosure)
SWIFT_DECLARE_MOVE_ONLY_INTEROP_WRAPPER(CxxOnceClosure, base::OnceClosure)
SWIFT_DECLARE_REF_COUNTED_HELPERS(ClosureProvider)

class ClosureProvider final : public base::RefCounted<ClosureProvider> {
 public:
  ClosureProvider();
  CxxRepeatingClosure GetRepeatingClosure();
  CxxOnceClosure GetOnceClosure();
  int call_count() { return call_count_; }
  static ClosureProvider* MakeForSwift() SWIFT_RETURNS_RETAINED;

 private:
  friend class base::RefCounted<ClosureProvider>;

  ~ClosureProvider();
  void Callback();
  int call_count_ = 0;
  base::WeakPtrFactory<ClosureProvider> weak_ptr_factory_{this};
} SWIFT_REF_COUNTED(ClosureProvider);

// We need a helper function to be able to call a OnceClosure from swift
// Unfortunately `(consume cb).Run()` in swift does not correctly convert
// `cb` to a non-const r-value for the C++ interop bindings.
void RunCxxOnceClosure(CxxOnceClosure&& cb);

#endif  // IOS_CHROME_TEST_SWIFT_INTEROP_INCLUDE_CLOSURE_H_
