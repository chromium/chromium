// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/crash_helpers.h"

#include "base/debug/alias.h"

namespace blink {

namespace internal {

NOINLINE void CrashIntentionally() {
  // NOTE(shess): Crash directly rather than using NOTREACHED_IN_MIGRATION() so
  // that the signature is easier to triage in crash reports.
  //
  // Linker's ICF feature may merge this function with other functions with the
  // same definition and it may confuse the crash report processing system.
  static int static_variable_to_make_this_function_unique = 0;
  base::debug::Alias(&static_variable_to_make_this_function_unique);

  volatile int* zero = nullptr;
  *zero = 0;
}

NOINLINE void BadCastCrashIntentionally() {
  class A {
    virtual void f() {}
  };

  class B {
    virtual void f() {}
  };

  A a;
  (void)(B*) & a;
}

}  // namespace internal

}  // namespace blink
