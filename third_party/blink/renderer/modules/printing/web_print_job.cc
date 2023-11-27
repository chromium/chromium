// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/printing/web_print_job.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_print_job_attributes.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

WebPrintJob::WebPrintJob(ExecutionContext* execution_context) {
  NOTIMPLEMENTED_LOG_ONCE();
}

WebPrintJob::~WebPrintJob() = default;

void WebPrintJob::Trace(Visitor* visitor) const {
  visitor->Trace(attributes_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
