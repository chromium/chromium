// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_MANAGED_DEVICE_HEALTH_SERVICES_TELEMETRY_CROS_TELEMETRY_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_MANAGED_DEVICE_HEALTH_SERVICES_TELEMETRY_CROS_TELEMETRY_H_

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_MANAGED_DEVICE_HEALTH_SERVICES_TELEMETRY_CROS_TELEMETRY_H_
