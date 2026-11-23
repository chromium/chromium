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

#include <optional>

#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        StrCat({"'", url, "' is not a valid URL."}));
    return KURL();
  }

  if (!execution_context->GetSecurityOrigin()->CanReadContent(script_url)) {
    if (!RuntimeEnabledFeatures::
            NoSynchronousThrowForCrossOriginBlockedWorkerEnabled()) {
      UseCounter::Count(execution_context,
                        WebFeature::kWorkerScriptURLFetchBlockedByCrossOrigin);
      // We can safely expose the URL in the following exceptions, as these
      // checks happen synchronously before redirection. JavaScript receives
      // no new information.
      exception_state.ThrowSecurityError(
          StrCat({"Script at '", script_url.ElidedString(),
                  "' cannot be accessed from origin '",
                  execution_context->GetSecurityOrigin()->ToString(), "'."}));
      return KURL();
    }
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
      return false;
    }
  }
  return true;
}

bool AbstractWorker::CheckCanReadScriptURLForNoThrow(const KURL& script_url) {
  ExecutionContext* execution_context = GetExecutionContext();
  if (RuntimeEnabledFeatures::
          NoSynchronousThrowForCrossOriginBlockedWorkerEnabled() &&
      !execution_context->GetSecurityOrigin()->CanReadContent(script_url)) {
    UseCounter::Count(execution_context,
                      WebFeature::kWorkerScriptURLFetchBlockedByCrossOrigin);

    // Add console error to provide the failure information.
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        StrCat({"Script at '", script_url.ElidedString(),
                "' cannot be accessed from origin '",
                execution_context->GetSecurityOrigin()->ToString(), "'."})));
    return false;
  }
  return true;
}

std::optional<network::ParsedPermissionsPolicy>
AbstractWorker::ParsePermissionsPolicy(const KURL& script_url,
                                       const String& allow) {
  ExecutionContext* execution_context = GetExecutionContext();
  const SecurityOrigin* self_origin = execution_context->GetSecurityOrigin();
  scoped_refptr<const SecurityOrigin> src_origin =
      SecurityOrigin::Create(script_url);
  PolicyParserMessageBuffer logger;
  network::ParsedPermissionsPolicy parsed_permissions_policy =
      PermissionsPolicyParser::ParseAttribute(allow, *self_origin, *src_origin,
                                              logger, execution_context);
  for (const auto& message : logger.GetMessages()) {
    execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther, message.level,
        message.content));
  }
  const network::PermissionsPolicy* permissions_policy =
      execution_context->GetSecurityContext().GetPermissionsPolicy();
  CHECK(permissions_policy);
  const url::Origin src_url_origin = src_origin->ToUrlOrigin();
  const bool is_isolated_context = execution_context->IsIsolatedContext();
  for (const auto& decl : parsed_permissions_policy) {
    if (decl.Contains(src_url_origin) &&
        !permissions_policy->CanDelegateFeatureToOrigin(decl.feature,
                                                        src_url_origin)) {
      execution_context->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kSecurity,
          mojom::blink::ConsoleMessageLevel::kError,
          StrCat({"Permissions-Policy feature '",
                  GetNameForFeature(decl.feature, is_isolated_context),
                  "' specified in the 'allow' option cannot be delegated "
                  "by the creating context to the worker script's "
                  "origin."})));
      return std::nullopt;
    }
  }
  return parsed_permissions_policy;
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
