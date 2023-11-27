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
#include "third_party/blink/renderer/modules/printing/web_printing_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

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

ScriptPromise WebPrinter::printJob(
    ScriptState* script_state,
    const String& job_name,
    const WebPrintDocumentDescription* document,
    const WebPrintJobTemplateAttributes* attributes,
    ExceptionState& exception_state) {
  // TODO(b/302505962): Implement this.
  return ScriptPromise();
}

}  // namespace blink
