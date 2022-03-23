// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_reporting.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

ScriptPromise HandleRegisterResult(
    ScriptState* script_state,
    ExceptionState& exception_state,
    AttributionSrcLoader::RegisterResult result) {
  switch (result) {
    case AttributionSrcLoader::RegisterResult::kSuccess:
      return ScriptPromise::CastUndefined(script_state);
    case AttributionSrcLoader::RegisterResult::kNotAllowed:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                        "Not allowed.");
      break;
    case AttributionSrcLoader::RegisterResult::kInsecureContext:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "Cannot execute in an insecure context.");
      break;
    case AttributionSrcLoader::RegisterResult::kInvalidProtocol:
      exception_state.ThrowTypeError("URL must have the HTTPS protocol.");
      break;
    case AttributionSrcLoader::RegisterResult::kUntrustworthyOrigin:
      exception_state.ThrowTypeError("URL must have a trustworthy origin.");
      break;
    case AttributionSrcLoader::RegisterResult::kFailedToRegister:
      exception_state.ThrowTypeError("Failed to register.");
      break;
  }

  return ScriptPromise();
}

}  // namespace

// static
const char AttributionReporting::kSupplementName[] = "AttributionReporting";

// static
AttributionReporting& AttributionReporting::attributionReporting(
    LocalDOMWindow& window) {
  AttributionReporting* attribution_reporting =
      Supplement<LocalDOMWindow>::From<AttributionReporting>(window);
  if (!attribution_reporting) {
    attribution_reporting = MakeGarbageCollected<AttributionReporting>(window);
    Supplement<LocalDOMWindow>::ProvideTo<AttributionReporting>(
        window, attribution_reporting);
  }
  return *attribution_reporting;
}

AttributionReporting::AttributionReporting(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

void AttributionReporting::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

ScriptPromise AttributionReporting::registerSource(
    ScriptState* script_state,
    const String& url,
    ExceptionState& exception_state) {
  LocalFrame* frame = GetSupplementable()->GetFrame();
  if (!frame) {
    return HandleRegisterResult(
        script_state, exception_state,
        AttributionSrcLoader::RegisterResult::kNotAllowed);
  }

  Document* document = GetSupplementable()->document();

  AttributionSrcLoader::RegisterResult result =
      frame->GetAttributionSrcLoader()->RegisterSources(
          document->CompleteURL(url));

  return HandleRegisterResult(script_state, exception_state, result);
}

}  // namespace blink
