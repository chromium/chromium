// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCOPED_SCROLL_PROMISE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCOPED_SCROLL_PROMISE_RESOLVER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_result.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// An RAII-style class that wraps the `ScriptPromiseResolver<ScrollResult>` (for
// a programmatic scroll request) to guarantee that the promise gets resolved
// when the object is no longer in scope.
class ScopedScrollPromiseResolver {
 public:
  explicit ScopedScrollPromiseResolver(
      ScriptPromiseResolver<ScrollResult>* resolver)
      : resolver_(resolver) {}

  ~ScopedScrollPromiseResolver() {
    if (resolver_) {
      resolver_->Resolve(ScrollResult::Create());
    }
  }

 private:
  Persistent<ScriptPromiseResolver<ScrollResult>> resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCOPED_SCROLL_PROMISE_RESOLVER_H_
