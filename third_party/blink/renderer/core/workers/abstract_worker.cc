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

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

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
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        StrCat({"'", url, "' is not a valid URL."}));
    return KURL();
  }

  // We can safely expose the URL in the following exceptions, as these checks
  // happen synchronously before redirection. JavaScript receives no new
  // information.
  if (!execution_context->GetSecurityOrigin()->CanReadContent(script_url)) {
    exception_state.ThrowSecurityError(
        StrCat({"Script at '", script_url.ElidedString(),
                "' cannot be accessed from origin '",
                execution_context->GetSecurityOrigin()->ToString(), "'."}));
    return KURL();
  }

  ContentSecurityPolicy* csp = execution_context->GetContentSecurityPolicy();
  if (csp &&
      !base::FeatureList::IsEnabled(features::kNoThrowForCSPBlockedWorker)) {
    if (!csp->AllowWorkerContextFromSource(script_url)) {
      UseCounter::Count(execution_context,
                        WebFeature::kCSPBlockedWorkerCreation);
      exception_state.ThrowSecurityError(
          StrCat({"Access to the script at '", script_url.ElidedString(),
                  "' is denied by the document's Content Security Policy."}));
      return KURL();
    }
  }

  return script_url;
}

bool AbstractWorker::CheckAllowedByCSPForNoThrow(const KURL& script_url) {
  ContentSecurityPolicy* csp =
      GetExecutionContext()->GetContentSecurityPolicy();
  if (csp &&
      base::FeatureList::IsEnabled(features::kNoThrowForCSPBlockedWorker)) {
    if (!csp->AllowWorkerContextFromSource(script_url)) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kCSPBlockedWorkerCreation);
      // In the spec, CSP check is part of fetch and should result in network
      // error and trigger error event asynchronously. See
      // https://www.w3.org/TR/CSP3/#fetch-integration.
      // Therefore, we dispatch an error event asynchronously.
      // This function is called during worker creation, if we dispatch an error
      // event immediately, the worker object has not been returned to the
      // script yet for it to have a chance to register an event handler.
      // Therefore, post a task to fire error event later so that the script
      // have a chance to register an event handler.
      GetExecutionContext()
          ->GetTaskRunner(TaskType::kInternalLoading)
          ->PostTask(FROM_HERE, BindOnce(&AbstractWorker::DispatchErrorEvent,
                                         WrapWeakPersistent(this)));
      return false;
    }
  }
  return true;
}

void AbstractWorker::DispatchErrorEvent() {
  DCHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  DispatchEvent(*Event::CreateCancelable(event_type_names::kError));
}

void AbstractWorker::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
