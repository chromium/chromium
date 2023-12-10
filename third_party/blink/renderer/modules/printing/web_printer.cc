// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/printing/web_printer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_print_document_description.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_print_job_template_attributes.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printer_attributes.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/printing/web_print_job.h"
#include "third_party/blink/renderer/modules/printing/web_printing_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

namespace {
bool ValidatePrintJobTemplateAttributes(
    const WebPrintJobTemplateAttributes* pjt_attributes,
    ExceptionState& exception_state) {
  if (pjt_attributes->hasCopies() && pjt_attributes->copies() < 1) {
    exception_state.ThrowTypeError("|copies| cannot be less than 1.");
  }
  return true;
}
}  // namespace

WebPrinter::WebPrinter(ExecutionContext* execution_context,
                       mojom::blink::WebPrinterInfoPtr printer_info)
    : printer_(execution_context) {
  printer_.Bind(std::move(printer_info->printer_remote),
                execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  attributes_ = WebPrinterAttributes::Create();
  attributes_->setPrinterName(printer_info->printer_name);
}

WebPrinter::~WebPrinter() = default;

void WebPrinter::Trace(Visitor* visitor) const {
  visitor->Trace(attributes_);
  visitor->Trace(fetch_attributes_resolver_);
  visitor->Trace(printer_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise WebPrinter::fetchAttributes(ScriptState* script_state,
                                          ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Context has shut down.");
    return ScriptPromise();
  }

  if (fetch_attributes_resolver_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "A call to fetchAttributes() is already in progress.");
    return ScriptPromise();
  }

  fetch_attributes_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  printer_->FetchAttributes(
      fetch_attributes_resolver_->WrapCallbackInScriptScope(
          WTF::BindOnce(&WebPrinter::OnFetchAttributes, WrapPersistent(this))));
  return fetch_attributes_resolver_->Promise();
}

ScriptPromise WebPrinter::printJob(
    ScriptState* script_state,
    const String& job_name,
    const WebPrintDocumentDescription* document,
    const WebPrintJobTemplateAttributes* pjt_attributes,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Context has shut down.");
    return ScriptPromise();
  }

  if (!ValidatePrintJobTemplateAttributes(pjt_attributes, exception_state)) {
    return ScriptPromise();
  }

  auto attributes =
      mojo::ConvertTo<mojom::blink::WebPrintJobTemplateAttributesPtr>(
          pjt_attributes);
  attributes->job_name = job_name;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  printer_->Print(document->data()->AsMojoBlob(), std::move(attributes),
                  resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                      &WebPrinter::OnPrint, WrapPersistent(this))));
  return resolver->Promise();
}

void WebPrinter::OnFetchAttributes(
    ScriptPromiseResolver*,
    mojom::blink::WebPrinterAttributesPtr printer_attributes) {
  if (!printer_attributes) {
    fetch_attributes_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNetworkError, "Unable to fetch printer attributes.");
    fetch_attributes_resolver_ = nullptr;
    return;
  }

  auto* new_attributes =
      mojo::ConvertTo<WebPrinterAttributes*>(printer_attributes);
  new_attributes->setPrinterName(attributes_->printerName());
  attributes_ = new_attributes;

  fetch_attributes_resolver_->Resolve(attributes_);
  fetch_attributes_resolver_ = nullptr;
}

void WebPrinter::OnPrint(ScriptPromiseResolver* resolver,
                         mojom::blink::WebPrintResultPtr result) {
  if (result->is_error()) {
    // TODO(b/302505962): Include returned error into the message.
    resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                     "Something went wrong during printing.");
    return;
  }

  auto* print_job = MakeGarbageCollected<WebPrintJob>(
      resolver->GetExecutionContext(), std::move(result->get_print_job_info()));
  resolver->Resolve(print_job);
}

}  // namespace blink
