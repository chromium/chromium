// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/webview/web_view.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

const char WebView::kSupplementName[] = "WebView";

WebView& WebView::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());

  auto* supplement =
      Supplement<ExecutionContext>::From<WebView>(execution_context);

  if (!supplement) {
    supplement = MakeGarbageCollected<WebView>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

WebView::WebView(ExecutionContext& execution_context)
    : Supplement<ExecutionContext>(execution_context),
      ExecutionContextClient(&execution_context) {}

void WebView::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
