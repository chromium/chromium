// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Useful utilities for testing V8 extras and streams.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_EXTRAS_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_EXTRAS_TEST_UTILS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class ScriptValue;
class V8TestingScope;

// If any exceptions are thrown while TryCatchScope is in scope, the test will
// fail.
class TryCatchScope {
  STACK_ALLOCATED();

 public:
  explicit TryCatchScope(v8::Isolate*);
  ~TryCatchScope();

 private:
  v8::TryCatch trycatch_;
};

// Evaluate the Javascript in "script" and return the result.
ScriptValue Eval(V8TestingScope*, const char* script);

// Evaluate the Javascript in "script" and return the result, failing the test
// if an exception is thrown.
ScriptValue EvalWithPrintingError(V8TestingScope*, const char* script);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_EXTRAS_TEST_UTILS_H_
