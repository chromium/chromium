// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/kiosk/cros_kiosk.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

const char CrosKiosk::kSupplementName[] = "CrosKiosk";

CrosKiosk& CrosKiosk::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());
  CrosKiosk* supplement =
      Supplement<ExecutionContext>::From<CrosKiosk>(execution_context);
  if (!supplement) {
    supplement = MakeGarbageCollected<CrosKiosk>(execution_context);
    ProvideTo(execution_context, supplement);
  }
  return *supplement;
}

CrosKiosk::CrosKiosk(ExecutionContext& execution_context)
    : Supplement(execution_context) {}

void CrosKiosk::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink
