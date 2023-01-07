/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/timing/performance_navigation.h"

#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"

// Legacy support for NT1(https://www.w3.org/TR/navigation-timing/).
namespace blink {

PerformanceNavigation::PerformanceNavigation(ExecutionContext* context)
    : ExecutionContextClient(context) {}

uint8_t PerformanceNavigation::type() const {
  if (!DomWindow())
    return kTypeNavigate;

  switch (DomWindow()->document()->Loader()->GetNavigationType()) {
    case kWebNavigationTypeReload:
    case kWebNavigationTypeFormResubmittedReload:
      return kTypeReload;
    case kWebNavigationTypeBackForward:
    case kWebNavigationTypeFormResubmittedBackForward:
      return kTypeBackForward;
    default:
      return kTypeNavigate;
  }
}

uint16_t PerformanceNavigation::redirectCount() const {
  if (!DomWindow())
    return 0;

  const DocumentLoadTiming& timing =
      DomWindow()->document()->Loader()->GetTiming();
  if (timing.HasCrossOriginRedirect())
    return 0;

  return timing.RedirectCount();
}

ScriptValue PerformanceNavigation::toJSONForBinding(
    ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  result.AddNumber("type", type());
  result.AddNumber("redirectCount", redirectCount());
  return result.GetScriptValue();
}

void PerformanceNavigation::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
