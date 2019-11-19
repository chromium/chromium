/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webmidi/navigator_web_midi.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webmidi/midi_access_initializer.h"
#include "third_party/blink/renderer/modules/webmidi/midi_options.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {
namespace {

const char kFeaturePolicyErrorMessage[] =
    "Midi has been disabled in this document by Feature Policy.";
const char kFeaturePolicyConsoleWarning[] =
    "Midi access has been blocked because of a Feature Policy applied to the "
    "current document. See https://goo.gl/EuHzyv for more details.";

}  // namespace

NavigatorWebMIDI::NavigatorWebMIDI(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

void NavigatorWebMIDI::Trace(blink::Visitor* visitor) {
  Supplement<Navigator>::Trace(visitor);
}

const char NavigatorWebMIDI::kSupplementName[] = "NavigatorWebMIDI";

NavigatorWebMIDI& NavigatorWebMIDI::From(Navigator& navigator) {
  NavigatorWebMIDI* supplement =
      Supplement<Navigator>::From<NavigatorWebMIDI>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorWebMIDI>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

ScriptPromise NavigatorWebMIDI::requestMIDIAccess(ScriptState* script_state,
                                                  Navigator& navigator,
                                                  const MIDIOptions* options) {
  return NavigatorWebMIDI::From(navigator).requestMIDIAccess(script_state,
                                                             options);
}

ScriptPromise NavigatorWebMIDI::requestMIDIAccess(ScriptState* script_state,
                                                  const MIDIOptions* options) {
  if (!script_state->ContextIsValid()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "The frame is not working."));
  }

  Document& document = *To<Document>(ExecutionContext::From(script_state));
  if (options->hasSysex() && options->sysex()) {
    UseCounter::Count(
        document,
        WebFeature::kRequestMIDIAccessWithSysExOption_ObscuredByFootprinting);
    document.CountUseOnlyInCrossOriginIframe(
        WebFeature::
            kRequestMIDIAccessIframeWithSysExOption_ObscuredByFootprinting);
  } else {
    // In the recent spec, the step 7 below allows user-agents to prompt the
    // user for permission regardless of sysex option.
    // https://webaudio.github.io/web-midi-api/#dom-navigator-requestmidiaccess
    // https://crbug.com/662000.
    if (document.IsSecureContext()) {
      Deprecation::CountDeprecation(
          document, WebFeature::kNoSysexWebMIDIWithoutPermission);
    }
  }
  document.CountUseOnlyInCrossOriginIframe(
      WebFeature::kRequestMIDIAccessIframe_ObscuredByFootprinting);

  if (!document.IsFeatureEnabled(mojom::FeaturePolicyFeature::kMidiFeature,
                                 ReportOptions::kReportOnFailure,
                                 kFeaturePolicyConsoleWarning)) {
    UseCounter::Count(document, WebFeature::kMidiDisabledByFeaturePolicy);
    return ScriptPromise::RejectWithDOMException(
        script_state,
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kSecurityError,
                                           kFeaturePolicyErrorMessage));
  }

  return MIDIAccessInitializer::Start(script_state, options);
}

}  // namespace blink
