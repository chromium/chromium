/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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

#include "third_party/blink/renderer/core/frame/window_or_worker_global_scope.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/scheduled_action.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_for_context_dispose.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/dom_timer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/page_dismissal_scope.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

static bool IsAllowed(ExecutionContext* execution_context,
                      bool is_eval,
                      const String& source) {
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    if (!window->GetFrame())
      return false;
    if (is_eval && !window->GetContentSecurityPolicy()->AllowEval(
                       ReportingDisposition::kReport,
                       ContentSecurityPolicy::kWillNotThrowException, source)) {
      return false;
    }
    if (PageDismissalScope::IsActive()) {
      UseCounter::Count(execution_context,
                        window->document()->ProcessingBeforeUnload()
                            ? WebFeature::kTimerInstallFromBeforeUnload
                            : WebFeature::kTimerInstallFromUnload);
    }
    return true;
  }
  if (execution_context->IsWorkerGlobalScope()) {
    WorkerGlobalScope* worker_global_scope =
        static_cast<WorkerGlobalScope*>(execution_context);
    if (!worker_global_scope->ScriptController())
      return false;
    ContentSecurityPolicy* policy =
        worker_global_scope->GetContentSecurityPolicy();
    if (is_eval && policy &&
        !policy->AllowEval(ReportingDisposition::kReport,
                           ContentSecurityPolicy::kWillNotThrowException,
                           source)) {
      return false;
    }
    return true;
  }
  NOTREACHED();
  return false;
}

String WindowOrWorkerGlobalScope::btoa(EventTarget&,
                                       const String& string_to_encode,
                                       ExceptionState& exception_state) {
  if (string_to_encode.IsNull())
    return String();

  if (!string_to_encode.ContainsOnlyLatin1OrEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The string to be encoded contains "
        "characters outside of the Latin1 range.");
    return String();
  }

  return Base64Encode(
      base::as_bytes(base::make_span(string_to_encode.Latin1())));
}

String WindowOrWorkerGlobalScope::atob(EventTarget&,
                                       const String& encoded_string,
                                       ExceptionState& exception_state) {
  if (encoded_string.IsNull())
    return String();

  if (!encoded_string.ContainsOnlyLatin1OrEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The string to be decoded contains "
        "characters outside of the Latin1 range.");
    return String();
  }
  Vector<char> out;
  if (!Base64Decode(encoded_string, out, IsHTMLSpace<UChar>,
                    kBase64ValidatePadding)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidCharacterError,
        "The string to be decoded is not correctly encoded.");
    return String();
  }

  return String(out.data(), out.size());
}

int WindowOrWorkerGlobalScope::setTimeout(
    ScriptState* script_state,
    EventTarget& event_target,
    V8Function* handler,
    int timeout,
    const HeapVector<ScriptValue>& arguments) {
  ExecutionContext* execution_context = event_target.GetExecutionContext();
  if (!IsAllowed(execution_context, false, g_empty_string))
    return 0;
  if (timeout >= 0 && execution_context->IsWindow()) {
    // FIXME: Crude hack that attempts to pass idle time to V8. This should
    // be done using the scheduler instead.
    V8GCForContextDispose::Instance().NotifyIdle();
  }
  auto* action = MakeGarbageCollected<ScheduledAction>(
      script_state, execution_context, handler, arguments);
  return DOMTimer::Install(execution_context, action,
                           base::TimeDelta::FromMilliseconds(timeout), true);
}

int WindowOrWorkerGlobalScope::setTimeout(ScriptState* script_state,
                                          EventTarget& event_target,
                                          const String& handler,
                                          int timeout,
                                          const HeapVector<ScriptValue>&) {
  ExecutionContext* execution_context = event_target.GetExecutionContext();
  if (!IsAllowed(execution_context, true, handler))
    return 0;
  // Don't allow setting timeouts to run empty functions.  Was historically a
  // performance issue.
  if (handler.IsEmpty())
    return 0;
  if (timeout >= 0 && execution_context->IsWindow()) {
    // FIXME: Crude hack that attempts to pass idle time to V8. This should
    // be done using the scheduler instead.
    V8GCForContextDispose::Instance().NotifyIdle();
  }
  auto* action = MakeGarbageCollected<ScheduledAction>(
      script_state, execution_context, handler);
  return DOMTimer::Install(execution_context, action,
                           base::TimeDelta::FromMilliseconds(timeout), true);
}

int WindowOrWorkerGlobalScope::setInterval(
    ScriptState* script_state,
    EventTarget& event_target,
    V8Function* handler,
    int timeout,
    const HeapVector<ScriptValue>& arguments) {
  ExecutionContext* execution_context = event_target.GetExecutionContext();
  if (!IsAllowed(execution_context, false, g_empty_string))
    return 0;
  auto* action = MakeGarbageCollected<ScheduledAction>(
      script_state, execution_context, handler, arguments);
  return DOMTimer::Install(execution_context, action,
                           base::TimeDelta::FromMilliseconds(timeout), false);
}

int WindowOrWorkerGlobalScope::setInterval(ScriptState* script_state,
                                           EventTarget& event_target,
                                           const String& handler,
                                           int timeout,
                                           const HeapVector<ScriptValue>&) {
  ExecutionContext* execution_context = event_target.GetExecutionContext();
  if (!IsAllowed(execution_context, true, handler))
    return 0;
  // Don't allow setting timeouts to run empty functions.  Was historically a
  // performance issue.
  if (handler.IsEmpty())
    return 0;
  auto* action = MakeGarbageCollected<ScheduledAction>(
      script_state, execution_context, handler);
  return DOMTimer::Install(execution_context, action,
                           base::TimeDelta::FromMilliseconds(timeout), false);
}

void WindowOrWorkerGlobalScope::clearTimeout(EventTarget& event_target,
                                             int timeout_id) {
  if (ExecutionContext* context = event_target.GetExecutionContext())
    DOMTimer::RemoveByID(context, timeout_id);
}

void WindowOrWorkerGlobalScope::clearInterval(EventTarget& event_target,
                                              int timeout_id) {
  if (ExecutionContext* context = event_target.GetExecutionContext())
    DOMTimer::RemoveByID(context, timeout_id);
}

ScriptPromise WindowOrWorkerGlobalScope::createImageBitmap(
    ScriptState* script_state,
    EventTarget&,
    const ImageBitmapSourceUnion& bitmap_source,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  return ImageBitmapFactories::CreateImageBitmap(script_state, bitmap_source,
                                                 options, exception_state);
}

ScriptPromise WindowOrWorkerGlobalScope::createImageBitmap(
    ScriptState* script_state,
    EventTarget&,
    const ImageBitmapSourceUnion& bitmap_source,
    int sx,
    int sy,
    int sw,
    int sh,
    const ImageBitmapOptions* options,
    ExceptionState& exception_state) {
  return ImageBitmapFactories::CreateImageBitmap(
      script_state, bitmap_source, sx, sy, sw, sh, options, exception_state);
}

bool WindowOrWorkerGlobalScope::crossOriginIsolated(
    const ExecutionContext& execution_context) {
  return execution_context.IsCrossOriginIsolated();
}

}  // namespace blink
