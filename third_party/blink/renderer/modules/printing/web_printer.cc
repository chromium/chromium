// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/printing/web_printer.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_web_printer_attributes.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

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
  visitor->Trace(printer_);
  visitor->Trace(attributes_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
