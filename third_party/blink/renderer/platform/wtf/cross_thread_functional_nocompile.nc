// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

struct ThreadUnsafeRefCounted : public base::RefCounted<ThreadUnsafeRefCounted> {
  void Method() {}
};

// Helper macro to work around https://crbug.com/1482675. The compiler only
// emits diagnostic messages when it first creates a template instantiation, so
// subsequent uses of a template with the same arguments will not print the
// expected errors. This macro creates a type that shadows its corresponding
// type from an ancestor scope, but is distinct for the purposes of
// instantiating a template.
#define DECLARE_UNIQUE(type, name) \
    class type : public ::blink::type {};\
    type name

void BindScopedRefptrToThreadUnsafeRefCounted() {
  scoped_refptr<ThreadUnsafeRefCounted> obj = base::MakeRefCounted<ThreadUnsafeRefCounted>();

  CrossThreadBindOnce(&ThreadUnsafeRefCounted::Method, obj);  // expected-error@*:* {{static assertion failed due to requirement 'functional_internal::kCheckNoThreadUnsafeRefCounted<scoped_refptr<blink::ThreadUnsafeRefCounted>>}}
  CrossThreadBindRepeating(&ThreadUnsafeRefCounted::Method, obj);  // expected-error@*:* {{static assertion failed due to requirement 'functional_internal::kCheckNoThreadUnsafeRefCounted<scoped_refptr<blink::ThreadUnsafeRefCounted>>}}
}

void RetainedRefToThreadUnsafeRefCounted() {
  scoped_refptr<ThreadUnsafeRefCounted> obj = base::MakeRefCounted<ThreadUnsafeRefCounted>();

  CrossThreadBindOnce(&ThreadUnsafeRefCounted::Method, base::RetainedRef(obj));  // expected-error@*:* {{static assertion failed due to requirement 'functional_internal::kCheckNoThreadUnsafeRefCounted<base::internal::RetainedRefWrapper<blink::ThreadUnsafeRefCounted>>}}
  CrossThreadBindRepeating(&ThreadUnsafeRefCounted::Method, base::RetainedRef(obj));  // expected-error@*:* {{static assertion failed due to requirement 'functional_internal::kCheckNoThreadUnsafeRefCounted<base::internal::RetainedRefWrapper<blink::ThreadUnsafeRefCounted>>}}

  CrossThreadBindOnce(&ThreadUnsafeRefCounted::Method, blink::RetainedRef(obj));  // expected-error@*:* {{functional_internal::kCheckNoThreadUnsafeRefCounted<blink::RetainedRefWrapper<blink::ThreadUnsafeRefCounted>>}}
  CrossThreadBindRepeating(&ThreadUnsafeRefCounted::Method, blink::RetainedRef(obj));  // expected-error@*:* {{functional_internal::kCheckNoThreadUnsafeRefCounted<blink::RetainedRefWrapper<blink::ThreadUnsafeRefCounted>>}}
}

}  // namespace blink
