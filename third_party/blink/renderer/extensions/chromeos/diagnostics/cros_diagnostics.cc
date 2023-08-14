// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/diagnostics/cros_diagnostics.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

const char CrosDiagnostics::kSupplementName[] = "CrosDiagnostics";

CrosDiagnostics& CrosDiagnostics::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());
  auto* supplement =
      Supplement<ExecutionContext>::From<CrosDiagnostics>(execution_context);
  if (!supplement) {
    supplement = MakeGarbageCollected<CrosDiagnostics>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

CrosDiagnostics::CrosDiagnostics(ExecutionContext& execution_context)
    : Supplement(execution_context),
      ExecutionContextClient(&execution_context) {}

void CrosDiagnostics::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
