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

void GarbageCollectedCannotBeUnretained() {
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    base::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    blink::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    blink::BindOnce([] (void*) {}, blink::Unretained(&obj));  // expected-error@*:* {{blink::Unretained() + GCed type is forbidden}}
  }
}

void GCMixinCannotBeUnretained() {
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    base::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    blink::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    blink::BindOnce([] (void*) {}, blink::Unretained(&obj));  // expected-error@*:* {{blink::Unretained() + GCed type is forbidden}}
  }
}

void GCImplWithMixinCannotBeUnretained() {
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    base::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    blink::BindOnce([] (void*) {}, base::Unretained(&obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    blink::BindOnce([] (void*) {}, blink::Unretained(&obj));  // expected-error@*:* {{blink::Unretained() + GCed type is forbidden}}
  }
}

void GarbageCollectedCannotBeBoundAsRawPointer(UnretainableObject* ptr) {
  base::BindOnce([] (void* ptr) {}, ptr);               // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  blink::BindOnce([] (UnretainableObject* ptr) {}, ptr);  // expected-error@*:* {{Raw pointers are not allowed to bind.}}
}

void GCMixinCannotBeBoundAsRawPointer(UnretainableMixin* ptr) {
  base::BindOnce([] (void* ptr) {}, ptr);  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  blink::BindOnce([] (void* ptr) {}, ptr);   // expected-error@*:* {{Raw pointers are not allowed to bind.}}
}

void GCImplWithmixinCannotBeBoundAsRawPointer(UnretainableImpl* ptr) {
  base::BindOnce([] (void* ptr) {}, ptr);             // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  blink::BindOnce([] (UnretainableImpl* ptr) {}, ptr);  // expected-error@*:* {{Raw pointers are not allowed to bind.}}
}

void GarbageCollectedCannotBeBoundByCref() {
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    base::BindOnce([] (const UnretainableObject& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    blink::BindOnce([] (const UnretainableObject& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GarbageCollectedCannotBeBoundByRef() {
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    base::BindOnce([] (const UnretainableObject& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableObject, obj);
    blink::BindOnce([] (const UnretainableObject& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GCMixinCannotBeBoundByCref() {
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    base::BindOnce([] (const UnretainableMixin& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    blink::BindOnce([] (const UnretainableMixin& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GCMixinCannotBeBoundByRef(UnretainableMixin& ref) {
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    base::BindOnce([] (const UnretainableMixin& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableMixin, obj);
    blink::BindOnce([] (const UnretainableMixin& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GCImplWithMixinCannotBeBoundByCref(UnretainableImpl& ref) {
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    base::BindOnce([] (const UnretainableImpl& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    blink::BindOnce([] (const UnretainableImpl& ref) {}, std::cref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

void GCImplWithMixinCannotBeBoundByRef(UnretainableImpl& ref) {
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    base::BindOnce([] (const UnretainableImpl& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
  {
    DECLARE_UNIQUE(UnretainableImpl, obj);
    blink::BindOnce([] (const UnretainableImpl& ref) {}, std::ref(obj));  // expected-error@*:* {{Argument requires unretained storage, but type does not support `Unretained()`.}}
  }
}

}  // namespace blink
