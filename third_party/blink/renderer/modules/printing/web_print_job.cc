// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/printing/web_print_job.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_web_print_job_attributes.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_print_job_state.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/printing/web_printing_type_converters.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

WebPrintJob::WebPrintJob(ExecutionContext* execution_context,
                         mojom::blink::WebPrintJobInfoPtr print_job_info)
    : ExecutionContextClient(execution_context),
      attributes_(MakeGarbageCollected<WebPrintJobAttributes>()),
      observer_(this, execution_context) {
  attributes_->setJobName(print_job_info->job_name);
  attributes_->setJobState(V8WebPrintJobState::Enum::kPreliminary);

  observer_.Bind(std::move(print_job_info->observer),
                 execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

WebPrintJob::~WebPrintJob() = default;

ExecutionContext* WebPrintJob::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& WebPrintJob::InterfaceName() const {
  return event_target_names::kWebPrintJob;
}

void WebPrintJob::OnWebPrintJobStateChanged(
    mojom::blink::WebPrintJobState state) {
  attributes_->setJobState(mojo::ConvertTo<V8WebPrintJobState::Enum>(state));
  DispatchEvent(*Event::Create(event_type_names::kJobstatechange));
}

void WebPrintJob::Trace(Visitor* visitor) const {
  visitor->Trace(attributes_);
  visitor->Trace(observer_);
  ExecutionContextClient::Trace(visitor);
  EventTarget::Trace(visitor);
}

}  // namespace blink
