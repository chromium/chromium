// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/shared_worker_client.h"

#include "base/check_op.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/workers/shared_worker.h"
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

void SharedWorkerClient::OnFeatureUsed(mojom::WebFeature feature) {
  UseCounter::Count(worker_->GetExecutionContext(), feature);
}

}  // namespace blink
