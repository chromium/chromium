// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This source file must contain a static destructor to check that
// the crazy linker can resolve weak symbols from the C library,
// like __aeabi_atexit(), which are not normally returned by
// a call to dlsym().

// Libc is not required to copy strings passed to putenv(). If it does
// not then env pointers become invalid when rodata is unmapped on
// library unload. To guard against this, putenv() strings are first
// strdup()'ed. This is a mild memory leak.

#include <stdlib.h>
#include <string.h>

#ifdef __arm__
extern "C" void __aeabi_atexit(void*);
#endif

class A {
 public:
  A() {
    x_ = rand();
    const char* env = getenv("TEST_VAR");
    if (!env || strcmp(env, "INIT"))
      putenv(strdup("TEST_VAR=LOAD_ERROR"));
    else
      putenv(strdup("TEST_VAR=LOADED"));
  }

  ~A() {
    const char* env = getenv("TEST_VAR");
    if (!env || strcmp(env, "LOADED"))
      putenv(strdup("TEST_VAR=UNLOAD_ERROR"));
    else
      putenv(strdup("TEST_VAR=UNLOADED"));
  }

  int Get() const { return x_; }

 private:
  int x_;
};

A s_a;

extern "C" int Foo() {
  return s_a.Get();
}
