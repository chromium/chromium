// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_receiver.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection_list.h"

namespace blink {

PresentationReceiver::PresentationReceiver(LocalDOMWindow* window)
    : connection_list_(
          MakeGarbageCollected<PresentationConnectionList>(window)),
      presentation_receiver_receiver_(this, window),
      presentation_service_remote_(window),
      window_(window) {
  DCHECK(window_->GetFrame()->IsOutermostMainFrame());
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      window->GetTaskRunner(TaskType::kPresentation);
  window->GetBrowserInterfaceBroker().GetInterface(
      presentation_service_remote_.BindNewPipeAndPassReceiver(task_runner));

  // Set the mojo::Remote<T> that remote implementation of PresentationService
  // will use to interact with the associated PresentationReceiver, in order
  // to receive updates on new connections becoming available.
  presentation_service_remote_->SetReceiver(
      presentation_receiver_receiver_.BindNewPipeAndPassRemote(task_runner));
}

ScriptPromise<PresentationConnectionList> PresentationReceiver::connectionList(
    ScriptState* script_state) {
  if (!connection_list_property_) {
    connection_list_property_ = MakeGarbageCollected<ConnectionListProperty>(
        ExecutionContext::From(script_state));
  }

  if (!connection_list_->IsEmpty() &&
      connection_list_property_->GetState() == ConnectionListProperty::kPending)
    connection_list_property_->Resolve(connection_list_);

  return connection_list_property_->Promise(script_state->World());
}

void PresentationReceiver::Terminate() {
  if (window_ && !window_->closed())
    window_->Close(window_.Get());
}

void PresentationReceiver::RemoveConnection(
    ReceiverPresentationConnection* connection) {
  DCHECK(connection_list_);
  connection_list_->RemoveConnection(connection);
}

void PresentationReceiver::OnReceiverConnectionAvailable(
    mojom::blink::PresentationConnectionResultPtr result) {
  // Take() will call PresentationReceiver::registerConnection()
  // and register the connection.
  auto* connection = ReceiverPresentationConnection::Take(
      this, *result->presentation_info, std::move(result->connection_remote),
      std::move(result->connection_receiver));

  // Only notify receiver.connectionList property if it has been acccessed
  // previously.
  if (!connection_list_property_)
    return;

  if (connection_list_property_->GetState() ==
      ConnectionListProperty::kPending) {
    connection_list_property_->Resolve(connection_list_);
  } else if (connection_list_property_->GetState() ==
             ConnectionListProperty::kResolved) {
    connection_list_->DispatchConnectionAvailableEvent(connection);
  }
}

void PresentationReceiver::RegisterConnection(
    ReceiverPresentationConnection* connection) {
  DCHECK(connection_list_);
  connection_list_->AddConnection(connection);
}

void PresentationReceiver::Trace(Visitor* visitor) const {
  visitor->Trace(connection_list_);
  visitor->Trace(connection_list_property_);
  visitor->Trace(presentation_receiver_receiver_);
  visitor->Trace(presentation_service_remote_);
  visitor->Trace(window_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
