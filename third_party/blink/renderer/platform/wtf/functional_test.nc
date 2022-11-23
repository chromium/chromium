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

// Classes deriving from Oilpan's garbage collected types should not be usable
// with base::Unretained.
class UnretainableObject : public GarbageCollected<UnretainableObject> {
};

class UnretainableMixin : public GarbageCollectedMixin {
};

class UnretainableImpl : public GarbageCollected<UnretainableImpl>, public UnretainableMixin {
};

#if defined(NCTEST_BASE_BIND_UNRETAINED_OBJECT)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableObject* ptr) {
  base::BindOnce([] (UnretainableObject* ptr) {}, base::Unretained(ptr));
}

#elif defined(NCTEST_WTF_BIND_UNRETAINED_OBJECT)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableObject* ptr) {
  WTF::BindOnce([] (UnretainableObject* ptr) {}, base::Unretained(ptr));
}

#elif defined(NCTEST_BASE_BIND_UNRETAINED_MIXIN)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableMixin* ptr) {
  base::BindOnce([] (UnretainableMixin* ptr) {}, base::Unretained(ptr));
}

#elif defined(NCTEST_WTF_BIND_UNRETAINED_MIXIN)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableMixin* ptr) {
  WTF::BindOnce([] (UnretainableMixin* ptr) {}, base::Unretained(ptr));
}

#elif defined(NCTEST_BASE_BIND_UNRETAINED_IMPL)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableImpl* ptr) {
  base::BindOnce([] (UnretainableImpl* ptr) {}, base::Unretained(ptr));
}

#elif defined(NCTEST_WTF_BIND_UNRETAINED_IMPL)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableImpl* ptr) {
  WTF::BindOnce([] (UnretainableImpl* ptr) {}, base::Unretained(ptr));
}

#elif defined(NCTEST_BASE_BIND_PTR_OBJECT)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableObject* ptr) {
  base::BindOnce([] (UnretainableObject* ptr) {}, ptr);
}

#elif defined(NCTEST_WTF_BIND_PTR_OBJECT)  // [r"fatal error: .+ Raw pointers are not allowed to bind into WTF::Function\. .+"]

void WontCompile(UnretainableObject* ptr) {
  WTF::BindOnce([] (UnretainableObject* ptr) {}, ptr);
}

#elif defined(NCTEST_BASE_BIND_PTR_MIXIN)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableMixin* ptr) {
  base::BindOnce([] (UnretainableMixin* ptr) {}, ptr);
}

#elif defined(NCTEST_WTF_BIND_PTR_MIXIN)  // [r"fatal error: .+ Raw pointers are not allowed to bind into WTF::Function\. .+"]

void WontCompile(UnretainableMixin* ptr) {
  WTF::BindOnce([] (UnretainableMixin* ptr) {}, ptr);
}

#elif defined(NCTEST_BASE_BIND_PTR_IMPL)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableImpl* ptr) {
  base::BindOnce([] (UnretainableImpl* ptr) {}, ptr);
}

#elif defined(NCTEST_WTF_BIND_PTR_IMPL)  // [r"fatal error: .+ Raw pointers are not allowed to bind into WTF::Function\. .+"]

void WontCompile(UnretainableImpl* ptr) {
  WTF::BindOnce([] (UnretainableImpl* ptr) {}, ptr);
}

#elif defined(NCTEST_BASE_BIND_CONST_REF_OBJECT)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableObject& ref) {
  base::BindOnce([] (const UnretainableObject& ref) {}, std::cref(ref));
}

#elif defined(NCTEST_WTF_BIND_CONST_REF_OBJECT)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableObject& ref) {
  WTF::BindOnce([] (const UnretainableObject& ref) {}, std::cref(ref));
}

#elif defined(NCTEST_BASE_BIND_CONST_REF_MIXIN)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableMixin& ref) {
  base::BindOnce([] (const UnretainableMixin& ref) {}, std::cref(ref));
}

#elif defined(NCTEST_WTF_BIND_CONST_REF_MIXIN)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableMixin& ref) {
  WTF::BindOnce([] (const UnretainableMixin& ref) {}, std::cref(ref));
}

#elif defined(NCTEST_BASE_BIND_CONST_REF_IMPL)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableImpl& ref) {
  base::BindOnce([] (const UnretainableImpl& ref) {}, std::cref(ref));
}

#elif defined(NCTEST_WTF_BIND_CONST_REF_IMPL)  // [r"fatal error: static assertion failed due to requirement 'TypeSupportsUnretainedV"]

void WontCompile(UnretainableImpl& ref) {
  WTF::BindOnce([] (const UnretainableImpl& ref) {}, std::cref(ref));
}

#endif

}  // namespace blink
