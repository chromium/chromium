// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/chromeos.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/extensions/chromeos/diagnostics/cros_diagnostics.h"
#include "third_party/blink/renderer/extensions/chromeos/kiosk/cros_kiosk.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

ChromeOS::ChromeOS() = default;

CrosDiagnostics* ChromeOS::diagnostics(ExecutionContext* execution_context) {
  return &CrosDiagnostics::From(*execution_context);
}

CrosKiosk* ChromeOS::kiosk(ExecutionContext* execution_context) {
  return &CrosKiosk::From(*execution_context);
}

void ChromeOS::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
