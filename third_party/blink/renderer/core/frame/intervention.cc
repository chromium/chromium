// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/intervention.h"

#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/intervention_report_body.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
void Intervention::GenerateReport(LocalFrame* frame,
                                  const String& id,
                                  const String& message) {
  if (!frame || !frame->Client())
    return;

  // Send the message to the console.
  auto* window = frame->DomWindow();
  window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kIntervention,
      mojom::ConsoleMessageLevel::kError, message));

  // Construct the intervention report.
  InterventionReportBody* body =
      MakeGarbageCollected<InterventionReportBody>(id, message);
  Report* report = MakeGarbageCollected<Report>(
      ReportType::kIntervention, window->document()->Url().GetString(), body);

  // Send the intervention report to the Reporting API and any
  // ReportingObservers.
  ReportingContext::From(window)->QueueReport(report);
}

}  // namespace blink
