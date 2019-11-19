// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

bool IsContextDestroyedForActiveScriptWrappable(
    const ExecutionContext* execution_context) {
  if (!execution_context)
    return true;

  if (execution_context->IsContextDestroyed())
    return true;

  if (const auto* doc = DynamicTo<Document>(execution_context)) {
    // Not all Document objects have an ExecutionContext that is actually
    // destroyed. In such cases we defer to the ContextDocument if possible.
    // If no such Document exists we consider the ExecutionContext as
    // destroyed. This is needed to ensure that an ActiveScriptWrappable that
    // always returns true in HasPendingActivity does not result in a memory
    // leak.
    if (const auto* context_doc = doc->ContextDocument())
      return context_doc->IsContextDestroyed();
    return true;
  }

  return false;
}

}  // namespace blink
