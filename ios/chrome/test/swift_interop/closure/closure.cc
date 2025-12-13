// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/swift_interop/include/closure.h"

SWIFT_DEFINE_INTEROP_WRAPPER(CxxRepeatingClosure, base::RepeatingClosure)
SWIFT_DEFINE_MOVE_ONLY_INTEROP_WRAPPER(CxxOnceClosure, base::OnceClosure)
SWIFT_DEFINE_REF_COUNTED_HELPERS(ClosureProvider)

ClosureProvider::ClosureProvider() = default;

ClosureProvider::~ClosureProvider() = default;

CxxRepeatingClosure ClosureProvider::GetRepeatingClosure() {
  return base::BindRepeating(&ClosureProvider::Callback,
                             weak_ptr_factory_.GetWeakPtr());
}

CxxOnceClosure ClosureProvider::GetOnceClosure() {
  return base::BindOnce(&ClosureProvider::Callback,
                        weak_ptr_factory_.GetWeakPtr());
}

void ClosureProvider::Callback() {
  call_count_++;
}

ClosureProvider* ClosureProvider::MakeForSwift() {
  // We inentionally leak the reference, which is adopted by swift.
  return base::MakeRefCounted<ClosureProvider>().release();
}

void RunCxxOnceClosure(CxxOnceClosure&& cb) {
  std::move(cb).Run();
}
