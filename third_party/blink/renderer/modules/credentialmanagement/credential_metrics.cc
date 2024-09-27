// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credential_metrics.h"

#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_timing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
const char CredentialMetrics::kSupplementName[] = "CredentialMetrics";

// static
CredentialMetrics& CredentialMetrics::From(ScriptState* script_state) {
  Document* document =
      To<LocalDOMWindow>(ExecutionContext::From(script_state))->document();
  CredentialMetrics* supplement =
      Supplement<Document>::From<CredentialMetrics>(document);
  if (!supplement) {
    supplement = MakeGarbageCollected<CredentialMetrics>(*document);
    ProvideTo(*document, supplement);
  }
  return *supplement;
}

CredentialMetrics::CredentialMetrics(Document& document)
    : Supplement<Document>(document) {}

CredentialMetrics::~CredentialMetrics() {}

void CredentialMetrics::RecordWebAuthnConditionalUiCall() {
  // It's not unexpected for conditional UI get requests to be called multiple
  // times on the same page. This measurement is only for the first one, which
  // should be immediately upon page load.
  if (conditional_ui_timing_reported_) {
    return;
  }

  Document* document = GetSupplementable();

  // UKMs can only be recorded for top-level frames.
  if (!document->GetFrame()->IsOutermostMainFrame()) {
    return;
  }

  conditional_ui_timing_reported_ = true;

  base::TimeDelta delta =
      base::TimeTicks::Now() - document->GetTiming().DomContentLoadedEventEnd();

  ukm::builders::WebAuthn_ConditionalUiGetCall(
      document->domWindow()->UkmSourceID())
      .SetTimeSinceDomContentLoaded(delta.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace blink
