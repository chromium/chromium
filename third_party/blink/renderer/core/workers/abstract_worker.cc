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

#include "third_party/blink/renderer/core/workers/abstract_worker.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

AbstractWorker::AbstractWorker(ExecutionContext* context)
    : ExecutionContextLifecycleStateObserver(context) {}

AbstractWorker::~AbstractWorker() = default;

// static
KURL AbstractWorker::ResolveURL(ExecutionContext* execution_context,
                                const String& url,
                                ExceptionState& exception_state) {
  KURL script_url = execution_context->CompleteURL(url);
  if (!script_url.IsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "'" + url + "' is not a valid URL.");
    return KURL();
  }

  // We can safely expose the URL in the following exceptions, as these checks
  // happen synchronously before redirection. JavaScript receives no new
  // information.
  if (!execution_context->GetSecurityOrigin()->CanReadContent(script_url)) {
    exception_state.ThrowSecurityError(
        "Script at '" + script_url.ElidedString() +
        "' cannot be accessed from origin '" +
        execution_context->GetSecurityOrigin()->ToString() + "'.");
    return KURL();
  }

  if (ContentSecurityPolicy* csp =
          execution_context->GetContentSecurityPolicy()) {
    if (!csp->AllowWorkerContextFromSource(script_url)) {
      exception_state.ThrowSecurityError(
          "Access to the script at '" + script_url.ElidedString() +
          "' is denied by the document's Content Security Policy.");
      return KURL();
    }
  }

  return script_url;
}

void AbstractWorker::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
