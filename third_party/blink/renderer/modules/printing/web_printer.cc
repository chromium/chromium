// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/printing/web_printer.h"
#include <limits>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_print_document_description.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_print_job_template_attributes.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printer_attributes.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_media_collection_requested.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_media_size_requested.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printing_resolution.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/printing/web_print_job.h"
#include "third_party/blink/renderer/modules/printing/web_printing_type_converters.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"

namespace blink {

namespace {

constexpr char kUserPermissionDeniedError[] =
    "User denied access to Web Printing API.";

constexpr char kPrinterUnreachableError[] = "Unable to connect to the printer.";

bool IsPositiveInt32(uint32_t value) {
  return value > 0 && value <= std::numeric_limits<int32_t>::max();
}

bool ValidatePrintJobTemplateAttributes(
    const WebPrintJobTemplateAttributes* pjt_attributes,
    ExceptionState& exception_state) {
  if (pjt_attributes->hasCopies() && pjt_attributes->copies() < 1) {
    exception_state.ThrowTypeError("|copies| cannot be less than 1.");
    return false;
  }
  if (pjt_attributes->hasPrinterResolution()) {
    auto* printer_resolution = pjt_attributes->printerResolution();
    if (!printer_resolution->hasCrossFeedDirectionResolution() ||
        !printer_resolution->hasFeedDirectionResolution()) {
      exception_state.ThrowTypeError(
          "crossFeedDirectionResolution and feedDirectionResolution must be "
          "specified if printerResolution is present.");
      return false;
    }
    if (printer_resolution->crossFeedDirectionResolution() == 0 ||
        printer_resolution->feedDirectionResolution() == 0) {
      exception_state.ThrowTypeError(
          "crossFeedDirectionResolution and feedDirectionResolution must be "
          "greater than 0 if specified.");
      return false;
    }
  }
  if (pjt_attributes->hasMediaCol()) {
    const auto& media_col = *pjt_attributes->mediaCol();
    const auto& media_size = *media_col.mediaSize();
    if (!IsPositiveInt32(media_size.yDimension()) ||
        !IsPositiveInt32(media_size.xDimension())) {
      exception_state.ThrowTypeError(
          "Both `xDimension` and `yDimension` must be positive integer "
          "values.");
      return false;
    }
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

ScriptPromise<WebPrinterAttributes> WebPrinter::fetchAttributes(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Context has shut down.");
    return EmptyPromise();
  }

  if (fetch_attributes_resolver_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "A call to fetchAttributes() is already in progress.");
    return EmptyPromise();
  }

  fetch_attributes_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<WebPrinterAttributes>>(
          script_state, exception_state.GetContext());
  printer_->FetchAttributes(
      fetch_attributes_resolver_->WrapCallbackInScriptScope(
          WTF::BindOnce(&WebPrinter::OnFetchAttributes, WrapPersistent(this))));
  return fetch_attributes_resolver_->Promise();
}

ScriptPromise<WebPrintJob> WebPrinter::printJob(
    ScriptState* script_state,
    const String& job_name,
    const WebPrintDocumentDescription* document,
    const WebPrintJobTemplateAttributes* pjt_attributes,
    ExceptionState& exception_state) {
  if (!script_state->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Context has shut down.");
    return EmptyPromise();
  }

  if (!ValidatePrintJobTemplateAttributes(pjt_attributes, exception_state)) {
    return EmptyPromise();
  }

  auto attributes =
      mojo::ConvertTo<mojom::blink::WebPrintJobTemplateAttributesPtr>(
          pjt_attributes);
  attributes->job_name = job_name;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<WebPrintJob>>(
      script_state, exception_state.GetContext());
  printer_->Print(document->data()->AsMojoBlob(), std::move(attributes),
                  resolver->WrapCallbackInScriptScope(WTF::BindOnce(
                      &WebPrinter::OnPrint, WrapPersistent(this))));
  return resolver->Promise();
}

void WebPrinter::OnFetchAttributes(
    ScriptPromiseResolver<WebPrinterAttributes>*,
    mojom::blink::WebPrinterFetchResultPtr result) {
  if (result->is_error()) {
    switch (result->get_error()) {
      case mojom::blink::WebPrinterFetchError::kPrinterUnreachable:
        fetch_attributes_resolver_->RejectWithDOMException(
            DOMExceptionCode::kNetworkError, kPrinterUnreachableError);
        break;
      case mojom::blink::WebPrinterFetchError::kUserPermissionDenied:
        fetch_attributes_resolver_->RejectWithDOMException(
            DOMExceptionCode::kNotAllowedError, kUserPermissionDeniedError);
        break;
    }
    fetch_attributes_resolver_ = nullptr;
    return;
  }

  auto* new_attributes = mojo::ConvertTo<WebPrinterAttributes*>(
      std::move(result->get_printer_attributes()));
  new_attributes->setPrinterName(attributes_->printerName());
  attributes_ = new_attributes;

  fetch_attributes_resolver_->Resolve(attributes_);
  fetch_attributes_resolver_ = nullptr;
}

void WebPrinter::OnPrint(ScriptPromiseResolver<WebPrintJob>* resolver,
                         mojom::blink::WebPrintResultPtr result) {
  if (result->is_error()) {
    switch (result->get_error()) {
      case mojom::blink::WebPrintError::kPrinterUnreachable:
        resolver->RejectWithDOMException(DOMExceptionCode::kNetworkError,
                                         kPrinterUnreachableError);
        break;
      case mojom::blink::WebPrintError::kPrintJobTemplateAttributesMismatch:
        resolver->RejectWithDOMException(
            DOMExceptionCode::kDataError,
            "The requested WebPrintJobTemplateAttributes do not align with the "
            "printer capabilities.");
        break;
      case mojom::blink::WebPrintError::kDocumentMalformed:
        resolver->RejectWithDOMException(DOMExceptionCode::kDataError,
                                         "The provided `data` is malformed.");
        break;
      case mojom::blink::WebPrintError::kUserPermissionDenied:
        resolver->RejectWithDOMException(DOMExceptionCode::kNotAllowedError,
                                         kUserPermissionDeniedError);
        break;
    }
    return;
  }

  auto* print_job = MakeGarbageCollected<WebPrintJob>(
      resolver->GetExecutionContext(), std::move(result->get_print_job_info()));
  resolver->Resolve(print_job);
}

}  // namespace blink
