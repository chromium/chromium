// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/is_return_type_compatible.h"

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

// No gtest tests; only static_assert checks.

namespace blink::bindings {

namespace {

// These tests focus on testing the cases that is_return_type_compatible
// should prevent from compiling.  The rest of the codebase constitutes a
// sufficient test of the things that should be allowed to compile.  However,
// the tests come in incompatible/compatible pairs to test that the false
// result is for the expected reason (and the test is still working as
// expected).

// A function returning `const DOMRectReadOnly*` should not be allowed when we
// expect the non-const type.
static_assert(!IsReturnTypeCompatible<DOMRectReadOnly, const DOMRectReadOnly*>);
static_assert(IsReturnTypeCompatible<DOMRectReadOnly, DOMRectReadOnly*>);

// similar to previous, but inside a sequence
static_assert(
    !IsReturnTypeCompatible<IDLSequence<DOMRectReadOnly>,
                            HeapVector<Member<const DOMRectReadOnly>>>);
static_assert(IsReturnTypeCompatible<IDLSequence<DOMRectReadOnly>,
                                     HeapVector<Member<DOMRectReadOnly>>>);

}  // namespace

}  // namespace blink::bindings
