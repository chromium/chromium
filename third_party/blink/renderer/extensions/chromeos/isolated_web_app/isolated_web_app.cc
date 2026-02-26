// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/isolated_web_app/isolated_web_app.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

const char IsolatedWebApp::kSupplementName[] = "IsolatedWebApp";

// static
IsolatedWebApp& IsolatedWebApp::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());
  IsolatedWebApp* supplement =
      Supplement<ExecutionContext>::From<IsolatedWebApp>(execution_context);
  if (!supplement) {
    supplement = MakeGarbageCollected<IsolatedWebApp>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

IsolatedWebApp::IsolatedWebApp(ExecutionContext& execution_context)
    : Supplement(execution_context) {}

void IsolatedWebApp::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
