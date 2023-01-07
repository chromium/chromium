// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

bool IsContextDestroyedForActiveScriptWrappable(
    const ExecutionContext* execution_context) {
  return !execution_context || execution_context->IsContextDestroyed();
}

}  // namespace blink
