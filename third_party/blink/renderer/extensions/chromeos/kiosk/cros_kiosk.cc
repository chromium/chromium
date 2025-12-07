// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/kiosk/cros_kiosk.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

CrosKiosk& CrosKiosk::From(ExecutionContext& execution_context) {
  CHECK(!execution_context.IsContextDestroyed());
  CrosKiosk* supplement = execution_context.GetCrosKiosk();
  if (!supplement) {
    supplement = MakeGarbageCollected<CrosKiosk>();
    execution_context.SetCrosKiosk(supplement);
  }
  return *supplement;
}

void CrosKiosk::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
