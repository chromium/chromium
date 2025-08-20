// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker_client.h"

#include "base/check_op.h"
#include "third_party/blink/public/mojom/worker/shared_worker_exception_details.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/shared_worker.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

SharedWorkerClient::SharedWorkerClient(SharedWorker* worker)
    : worker_(worker) {}

SharedWorkerClient::~SharedWorkerClient() {
  // We have lost our connection to the worker. If this happens before
  // OnConnected() is called, then it suggests that the document is gone or
  // going away.
}

void SharedWorkerClient::OnCreated(
    mojom::SharedWorkerCreationContextType creation_context_type) {
  worker_->SetIsBeingConnected(true);

  // No nested workers (for now) - connect() can only be called from a
  // window context.
  DCHECK(worker_->GetExecutionContext()->IsWindow());
  DCHECK_EQ(creation_context_type,
            worker_->GetExecutionContext()->IsSecureContext()
                ? mojom::SharedWorkerCreationContextType::kSecure
                : mojom::SharedWorkerCreationContextType::kNonsecure);
}

void SharedWorkerClient::OnConnected(
    const Vector<mojom::WebFeature>& features_used) {
  worker_->SetIsBeingConnected(false);
  for (auto feature : features_used)
    OnFeatureUsed(feature);
}

void SharedWorkerClient::OnScriptLoadFailed(const String& error_message) {
  worker_->SetIsBeingConnected(false);
  if (!error_message.empty()) {
    worker_->GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kWorker,
            mojom::blink::ConsoleMessageLevel::kError, error_message));
  }
  worker_->DispatchEvent(*Event::CreateCancelable(event_type_names::kError));
  // |this| can be destroyed at this point, for example, when a frame hosting
  // this shared worker is detached in the error handler, and closes mojo's
  // strong bindings bound with |this| in
  // SharedWorkerClientHolder::ContextDestroyed().
}

void SharedWorkerClient::OnReportException(
    mojom::blink::SharedWorkerExceptionDetailsPtr details) {
  auto* location = MakeGarbageCollected<SourceLocation>(
      details->source_location->url, /*char_position=*/0,
      details->source_location->line, details->source_location->column);
  worker_->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kWorker,
          mojom::blink::ConsoleMessageLevel::kError, details->error_message,
          location));

  // The HTML spec dictates which type of event should be dispatched for worker
  // errors.
  // - For script fetch/parse errors, a generic `Event` is dispatched.
  //   See:
  //   https://html.spec.whatwg.org/multipage/workers.html#worker-processing-model
  // - For runtime script errors during evaluation, an `ErrorEvent` is
  //   dispatched.
  //   See:
  //   https://html.spec.whatwg.org/multipage/webappapis.html#report-an-exception
  if (details->error_type ==
      mojom::blink::SharedWorkerErrorType::kRuntimeError) {
    ErrorEvent* event =
        ErrorEvent::Create(details->error_message, location, /*world=*/nullptr);
    worker_->DispatchEvent(*event);
  } else {
    worker_->DispatchEvent(*Event::CreateCancelable(event_type_names::kError));
  }
}

void SharedWorkerClient::OnFeatureUsed(mojom::WebFeature feature) {
  UseCounter::Count(worker_->GetExecutionContext(), feature);
}

}  // namespace blink
