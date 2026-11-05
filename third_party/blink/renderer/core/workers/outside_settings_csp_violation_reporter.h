// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_OUTSIDE_SETTINGS_CSP_VIOLATION_REPORTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_OUTSIDE_SETTINGS_CSP_VIOLATION_REPORTER_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/security_context/outside_context_csp_violation_reporter.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver_set.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ExecutionContext;

// Receives the violations that the workers and the worklets detect while
// fetching with their outside settings object, and reports them on the
// execution context of the outside settings object. See
// mojom::blink::OutsideContextCSPViolationReporter.
class CORE_EXPORT OutsideSettingsCSPViolationReporter final
    : public GarbageCollected<OutsideSettingsCSPViolationReporter>,
      public mojom::blink::OutsideContextCSPViolationReporter,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  // Returns a new mojo remote for a worker or a worklet that the context
  // creates, or a null remote if reporting on the context is not available.
  static mojo::PendingRemote<mojom::blink::OutsideContextCSPViolationReporter>
  BindNewRemote(ExecutionContext& context);

  explicit OutsideSettingsCSPViolationReporter(ExecutionContext& context);

  // mojom::blink::OutsideContextCSPViolationReporter:
  void ReportCSPViolation(
      network::mojom::blink::CSPViolationPtr violation) override;

  void Trace(Visitor* visitor) const override;

 private:
  HeapMojoReceiverSet<mojom::blink::OutsideContextCSPViolationReporter,
                      OutsideSettingsCSPViolationReporter>
      receivers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_OUTSIDE_SETTINGS_CSP_VIOLATION_REPORTER_H_
