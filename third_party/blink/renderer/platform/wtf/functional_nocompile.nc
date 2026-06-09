// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <utility>

#include "base/functional/bind.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class StackAllocatedObject {
   STACK_ALLOCATED();
};

class TraceableObject {
 public:
  void Trace(Visitor*) const {}
};

class UnretainableObject : public GarbageCollected<UnretainableObject> {};
class UnretainableMixin : public GarbageCollectedMixin {};
class UnretainableImpl : public GarbageCollected<UnretainableImpl>,
                         public GarbageCollectedMixin {};

// Helper macro to work around https://crbug.com/1482675. The compiler only
// emits diagnostic messages when it first creates a template instantiation, so
// subsequent uses of a template with the same arguments will not print the
// expected errors. This macro creates a type that shadows its corresponding
// type from an ancestor scope, but is distinct for the purposes of
// instantiating a template.
#define DECLARE_UNIQUE(type, name) \
    class type : public ::blink::type {};\
    type name

void StackAllocatedCannotBeUnretained() {
  {
    DECLARE_UNIQUE(StackAllocatedObject, obj);
    std::ignore = base::BindOnce([] (void*) {}, Unretained(&obj));  // expected-error@*:* {{blink::Unretained() with GCed, traceable or stack-allocated type is forbidden}}
  }
  {
    DECLARE_UNIQUE(StackAllocatedObject, obj);
    std::ignore = blink::BindOnce([] (void*) {}, Unretained(&obj));  // expected-error@*:* {{blink::Unretained() with GCed, traceable or stack-allocated type is forbidden}}
  }
}

void TraceableCannotBeUnretained() {
  {
    DECLARE_UNIQUE(TraceableObject, obj);
    std::ignore = base::BindOnce([] (void*) {}, Unretained(&obj));  // expected-error@*:* {{blink::Unretained() with GCed, traceable or stack-allocated type is forbidden}}
  }
  {
    DECLARE_UNIQUE(TraceableObject, obj);
    std::ignore = blink::BindOnce([] (void*) {}, Unretained(&obj));  // expected-error@*:* {{blink::Unretained() with GCed, traceable or stack-allocated type is forbidden}}
  }
}

void GarbageCollectedCannotBeUnretained() {
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    std::ignore = base::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    std::ignore = blink::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    std::ignore = blink::BindOnce([] (void*) {}, blink::Unretained(&obj));  // expected-error@*:* {{blink::Unretained() with GCed, traceable or stack-allocated type is forbidden}}
  }
}

void GarbageCollectedCannotBeUnretainedException() {
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    std::ignore = base::BindOnce([] (void*) {}, blink::subtle::UnretainedException(&obj));  // expected-error@*:* {{UnretainedException() may only be applied to non-GC'd types.}}
  }
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    std::ignore = blink::BindOnce([] (void*) {}, blink::subtle::UnretainedException(&obj));  // expected-error@*:* {{UnretainedException() may only be applied to non-GC'd types.}}
  }
}

void GCMixinCannotBeUnretained() {
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    std::ignore = base::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    std::ignore = blink::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    std::ignore = blink::BindOnce([] (void*) {}, blink::Unretained(&obj));  // expected-error@*:* {{blink::Unretained() with GCed, traceable or stack-allocated type is forbidden}}
  }
}

void GCMixinCannotBeUnretainedException() {
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    std::ignore = base::BindOnce([] (void*) {}, blink::subtle::UnretainedException(&obj));  // expected-error@*:* {{UnretainedException() may only be applied to non-GC'd types.}}
  }
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    std::ignore = blink::BindOnce([] (void*) {}, blink::subtle::UnretainedException(&obj));  // expected-error@*:* {{UnretainedException() may only be applied to non-GC'd types.}}
  }
}

void GCImplWithMixinCannotBeUnretained() {
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    std::ignore = base::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    std::ignore = blink::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    std::ignore = blink::BindOnce([] (void*) {}, blink::Unretained(&obj));  // expected-error@*:* {{blink::Unretained() with GCed, traceable or stack-allocated type is forbidden}}
  }
}

void GCImplWithMixinCannotBeUnretainedException() {
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    std::ignore = base::BindOnce([] (void*) {}, blink::subtle::UnretainedException(&obj));  // expected-error@*:* {{UnretainedException() may only be applied to non-GC'd types.}}
  }
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    std::ignore = blink::BindOnce([] (void*) {}, blink::subtle::UnretainedException(&obj));  // expected-error@*:* {{UnretainedException() may only be applied to non-GC'd types.}}
  }
}

void GarbageCollectedCannotBeBoundAsRawPointer(UnretainableObject* ptr) {
  std::ignore = base::BindOnce([] (void* ptr) {}, ptr);               // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  std::ignore = blink::BindOnce([] (UnretainableObject* ptr) {}, ptr);  // expected-error@*:* {{Raw pointers are not allowed to bind.}}
}

void GCMixinCannotBeBoundAsRawPointer(UnretainableMixin* ptr) {
  std::ignore = base::BindOnce([] (void* ptr) {}, ptr);  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  std::ignore = blink::BindOnce([] (void* ptr) {}, ptr);   // expected-error@*:* {{Raw pointers are not allowed to bind.}}
}

void GCImplWithmixinCannotBeBoundAsRawPointer(UnretainableImpl* ptr) {
  std::ignore = base::BindOnce([] (void* ptr) {}, ptr);             // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  std::ignore = blink::BindOnce([] (UnretainableImpl* ptr) {}, ptr);  // expected-error@*:* {{Raw pointers are not allowed to bind.}}
}

void GarbageCollectedCannotBeBoundByCref() {
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    std::ignore = base::BindOnce([] (const UnretainableObject& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    std::ignore = blink::BindOnce([] (const UnretainableObject& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GarbageCollectedCannotBeBoundByRef() {
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    std::ignore = base::BindOnce([] (const UnretainableObject& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    std::ignore = blink::BindOnce([] (const UnretainableObject& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GCMixinCannotBeBoundByCref() {
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    std::ignore = base::BindOnce([] (const UnretainableMixin& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    std::ignore = blink::BindOnce([] (const UnretainableMixin& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GCMixinCannotBeBoundByRef(UnretainableMixin& ref) {
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    std::ignore = base::BindOnce([] (const UnretainableMixin& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    std::ignore = blink::BindOnce([] (const UnretainableMixin& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GCImplWithMixinCannotBeBoundByCref(UnretainableImpl& ref) {
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    std::ignore = base::BindOnce([] (const UnretainableImpl& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    std::ignore = blink::BindOnce([] (const UnretainableImpl& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GCImplWithMixinCannotBeBoundByRef(UnretainableImpl& ref) {
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    std::ignore = base::BindOnce([] (const UnretainableImpl& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    std::ignore = blink::BindOnce([] (const UnretainableImpl& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

}  // namespace blink
