// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_MANAGED_DEVICE_HEALTH_SERVICES_TELEMETRY_CROS_TELEMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_MANAGED_DEVICE_HEALTH_SERVICES_TELEMETRY_CROS_TELEMETRY_H_

#include "third_party/blink/public/mojom/chromeos/system_extensions/managed_device_health_services/telemetry/cros_telemetry.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class CrosTelemetry : public ScriptWrappable,
                      public Supplement<ExecutionContext>,
                      public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static CrosTelemetry& From(ExecutionContext&);

  explicit CrosTelemetry(ExecutionContext&);

  // GarbageCollected:
  void Trace(Visitor*) const override;

  // Returns the remote for communication with the browser's telemetry
  // service implementation. Returns null if the ExecutionContext is
  // being destroyed or is not called by a system extension with the
  // ManagedDeviceHealthServices type.
  mojom::blink::CrosTelemetry* GetCrosTelemetryServiceOrNull();

 private:
  HeapMojoRemote<mojom::blink::CrosTelemetry> remote_telemetry_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_MANAGED_DEVICE_HEALTH_SERVICES_TELEMETRY_CROS_TELEMETRY_H_
