// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/printing/web_printing_manager.h"

#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/modules/printing/web_printer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

bool CheckContextAndPermissions(ScriptState* script_state,
                                ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Current context is detached.");
    return false;
  }

  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  if (!execution_context->IsIsolatedContext() ||
      !execution_context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kCrossOriginIsolated)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Frame is not sufficiently isolated to use Web Printing.");
    return false;
  }

  if (!execution_context->IsFeatureEnabled(
          mojom::blink::PermissionsPolicyFeature::kWebPrinting)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Permissions-Policy: web-printing is disabled.");
    return false;
  }

  return true;
}

}  // namespace

const char WebPrintingManager::kSupplementName[] = "PrintingManager";

WebPrintingManager* WebPrintingManager::GetWebPrintingManager(
    NavigatorBase& navigator) {
  WebPrintingManager* printing_manager =
      Supplement<NavigatorBase>::From<WebPrintingManager>(navigator);
  if (!printing_manager) {
    printing_manager = MakeGarbageCollected<WebPrintingManager>(navigator);
    ProvideTo(navigator, printing_manager);
  }
  return printing_manager;
}

WebPrintingManager::WebPrintingManager(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      printing_service_(navigator.GetExecutionContext()) {}

ScriptPromise<IDLSequence<WebPrinter>> WebPrintingManager::getPrinters(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!CheckContextAndPermissions(script_state, exception_state)) {
    return ScriptPromise<IDLSequence<WebPrinter>>();
  }

  auto* service = GetPrintingService();
  if (!service) {
    exception_state.ThrowSecurityError(
        "WebPrinting API is not accessible in this configuration.");
    return ScriptPromise<IDLSequence<WebPrinter>>();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLSequence<WebPrinter>>>(
          script_state, exception_state.GetContext());
  service->GetPrinters(resolver->WrapCallbackInScriptScope(WTF::BindOnce(
      &WebPrintingManager::OnPrintersRetrieved, WrapPersistent(this))));
  return resolver->Promise();
}

void WebPrintingManager::Trace(Visitor* visitor) const {
  visitor->Trace(printing_service_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
}

mojom::blink::WebPrintingService* WebPrintingManager::GetPrintingService() {
#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  if (!printing_service_.is_bound()) {
    auto* execution_context = GetSupplementable()->GetExecutionContext();
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        printing_service_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  return printing_service_.get();
#else
  return nullptr;
#endif
}

void WebPrintingManager::OnPrintersRetrieved(
    ScriptPromiseResolver<IDLSequence<WebPrinter>>* resolver,
    mojom::blink::GetPrintersResultPtr result) {
  if (result->is_error()) {
    switch (result->get_error()) {
      case mojom::blink::GetPrintersError::kUserPermissionDenied:
        resolver->RejectWithDOMException(
            DOMExceptionCode::kNotAllowedError,
            "User denied access to Web Printing API.");
        break;
    }
    return;
  }
  HeapVector<Member<WebPrinter>> printers;
  for (auto& printer_info : result->get_printers()) {
    printers.push_back(MakeGarbageCollected<WebPrinter>(
        GetSupplementable()->GetExecutionContext(), std::move(printer_info)));
  }
  resolver->Resolve(printers);
}

}  // namespace blink
