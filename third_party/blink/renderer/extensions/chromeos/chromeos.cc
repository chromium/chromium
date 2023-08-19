// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/chromeos.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/extensions/chromeos/diagnostics/cros_diagnostics.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/hid/cros_hid.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/managed_device_health_services/telemetry/cros_telemetry.h"
#include "third_party/blink/renderer/extensions/chromeos/system_extensions/window_management/cros_window_management.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

ChromeOS::ChromeOS() = default;

CrosWindowManagement* ChromeOS::windowManagement(
    ExecutionContext* execution_context) {
  return &CrosWindowManagement::From(*execution_context);
}

CrosHID* ChromeOS::hid(ExecutionContext* execution_context) {
  return &CrosHID::From(*execution_context);
}

CrosTelemetry* ChromeOS::telemetry(ExecutionContext* execution_context) {
  return &CrosTelemetry::From(*execution_context);
}

CrosDiagnostics* ChromeOS::diagnostics(ExecutionContext* execution_context) {
  return &CrosDiagnostics::From(*execution_context);
}

void ChromeOS::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
