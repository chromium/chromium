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

namespace {

using V8JobStateEnum = V8WebPrintJobState::Enum;

bool AreFurtherStateUpdatesPossible(V8JobStateEnum state) {
  switch (state) {
    case V8JobStateEnum::kCompleted:
    case V8JobStateEnum::kAborted:
    case V8JobStateEnum::kCanceled:
      // These states are terminal -- no more updates from the browser.
      return false;
    case V8JobStateEnum::kPreliminary:
    case V8JobStateEnum::kPending:
    case V8JobStateEnum::kProcessing:
      return true;
  }
}

}  // namespace

WebPrintJob::WebPrintJob(ExecutionContext* execution_context,
                         mojom::blink::WebPrintJobInfoPtr print_job_info)
    : ActiveScriptWrappable<WebPrintJob>({}),
      ExecutionContextClient(execution_context),
      attributes_(MakeGarbageCollected<WebPrintJobAttributes>()),
      observer_(this, execution_context),
      controller_(execution_context) {
  attributes_->setJobName(print_job_info->job_name);
  attributes_->setJobPages(print_job_info->job_pages);
  attributes_->setJobPagesCompleted(0);
  attributes_->setJobState(V8JobStateEnum::kPreliminary);

  observer_.Bind(std::move(print_job_info->observer),
                 execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  controller_.Bind(
      std::move(print_job_info->controller),
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI));
}

WebPrintJob::~WebPrintJob() = default;

void WebPrintJob::cancel() {
  // There's no sense in cancelling a job that has either already been cancelled
  // or is in a terminal state.
  if (cancel_called_ ||
      !AreFurtherStateUpdatesPossible(attributes_->jobState().AsEnum())) {
    return;
  }
  cancel_called_ = true;
  controller_->Cancel();
}

ExecutionContext* WebPrintJob::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

const AtomicString& WebPrintJob::InterfaceName() const {
  return event_target_names::kWebPrintJob;
}

void WebPrintJob::OnWebPrintJobUpdate(
    mojom::blink::WebPrintJobUpdatePtr update) {
  auto state = mojo::ConvertTo<V8JobStateEnum>(update->state);
  // Discard the update if nothing has actually changed.
  if (state == attributes_->jobState().AsEnum() &&
      update->pages_printed == attributes_->jobPagesCompleted()) {
    return;
  }
  attributes_->setJobState(state);
  attributes_->setJobPagesCompleted(update->pages_printed);
  DispatchEvent(*Event::Create(event_type_names::kJobstatechange));
}

bool WebPrintJob::HasPendingActivity() const {
  // The job is kept alive for as long as there are more updates to be reported
  // and at least one listener to catch them.
  return AreFurtherStateUpdatesPossible(attributes_->jobState().AsEnum()) &&
         HasEventListeners();
}

void WebPrintJob::Trace(Visitor* visitor) const {
  visitor->Trace(attributes_);
  visitor->Trace(observer_);
  visitor->Trace(controller_);
  ExecutionContextClient::Trace(visitor);
  EventTarget::Trace(visitor);
}

}  // namespace blink
