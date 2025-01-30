// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "third_party/blink/renderer/platform/heap/heap_bind.h"

#include "base/functional/bind.h"
#include "base/functional/function_ref.h"

namespace blink::bindings {

void BindCapturingLambda() {
  int i;
  std::ignore = HeapBind([&i](int a ) { i = a; }, 42);  // expected-error@third_party/blink/renderer/platform/heap/heap_bind.h:* {{Capturing lambdas can't be bound}}
}

void PointerToNonGCTypeStruct() {
  struct A {
    void Method();
  };
  std::ignore = HeapBind(&A::Method, new A());  // expected-error@third_party/blink/renderer/platform/heap/heap_bind.h:* {{Only pointers to GarbageCollected types may be bound}}
}

void PointerToNonGCTypeInt() {
  std::ignore = HeapBind([](int* x) {}, nullptr);  // expected-error@third_party/blink/renderer/platform/heap/heap_bind.h:* {{Only pointers to GarbageCollected types may be bound}}
}

struct GCType : public GarbageCollected<GCType> {
  void Trace(Visitor*) const {}
};

void BindingGCTypeByValue() {
  std::ignore = HeapBind([](GCType* s) {}, nullptr);  // OK
  std::ignore = HeapBind([](GCType& s) {}, *MakeGarbageCollected<GCType>());  // expected-error@third_party/blink/renderer/platform/heap/heap_bind.h:* {{GarbageCollected classes should be bound as pointers}}
}

void BindFunctionRef() {
  [](base::FunctionRef<void()> ref) { HeapBind(ref); }([] {}); // expected-error@third_party/blink/renderer/platform/heap/heap_bind.h:* {{base::FunctionRef<> can't be bound}}
}

}  // namespace blink::bindings
