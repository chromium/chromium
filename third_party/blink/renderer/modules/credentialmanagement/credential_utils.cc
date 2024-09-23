// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

bool CheckGenericSecurityRequirementsForCredentialsContainerRequest(
    ScriptPromiseResolverBase* resolver) {
  // Ignore calls if the current realm execution context is no longer valid,
  // e.g., because the responsible document was detached.
  if (!resolver->GetExecutionContext()) {
    return false;
  }

  // The API is not exposed to Workers or Worklets, so if the current realm
  // execution context is valid, it must have a responsible browsing context.
  auto* window = To<LocalDOMWindow>(resolver->GetExecutionContext());

  // The API is not exposed in non-secure context.
  SECURITY_CHECK(window->IsSecureContext());

  if (window->GetFrame()->IsInFencedFrameTree()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "The credential operation is not allowed in a fenced frame tree."));
    return false;
  }

  return true;
}

}  // namespace blink
