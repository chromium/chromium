// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_callbacks.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_observer.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

PresentationController::PresentationController(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      presentation_service_remote_(&window),
      presentation_controller_receiver_(this, &window) {}

PresentationController::~PresentationController() = default;

// static
const char PresentationController::kSupplementName[] = "PresentationController";

// static
PresentationController* PresentationController::From(LocalDOMWindow& window) {
  PresentationController* controller =
      Supplement<LocalDOMWindow>::From<PresentationController>(window);
  if (!controller) {
    controller = MakeGarbageCollected<PresentationController>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, controller);
  }
  return controller;
}

// static
PresentationController* PresentationController::FromContext(
    ExecutionContext* execution_context) {
  if (!execution_context || execution_context->IsContextDestroyed())
    return nullptr;
  return From(*To<LocalDOMWindow>(execution_context));
}

void PresentationController::Trace(Visitor* visitor) const {
  visitor->Trace(presentation_controller_receiver_);
  visitor->Trace(presentation_);
  visitor->Trace(connections_);
  visitor->Trace(availability_state_);
  visitor->Trace(presentation_service_remote_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void PresentationController::SetPresentation(Presentation* presentation) {
  presentation_ = presentation;
}

void PresentationController::RegisterConnection(
    ControllerPresentationConnection* connection) {
  connections_.insert(connection);
}

PresentationAvailabilityState* PresentationController::GetAvailabilityState() {
  if (!availability_state_) {
    availability_state_ = MakeGarbageCollected<PresentationAvailabilityState>(
        GetPresentationService().get());
  }

  return availability_state_.Get();
}

void PresentationController::AddAvailabilityObserver(
    PresentationAvailabilityObserver* observer) {
  GetAvailabilityState()->AddObserver(observer);
}

void PresentationController::RemoveAvailabilityObserver(
    PresentationAvailabilityObserver* observer) {
  GetAvailabilityState()->RemoveObserver(observer);
}

void PresentationController::OnScreenAvailabilityUpdated(
    const KURL& url,
    mojom::blink::ScreenAvailability availability) {
  GetAvailabilityState()->UpdateAvailability(url, availability);
}

void PresentationController::OnConnectionStateChanged(
    mojom::blink::PresentationInfoPtr presentation_info,
    mojom::blink::PresentationConnectionState state) {
  PresentationConnection* connection = FindConnection(*presentation_info);
  if (!connection)
    return;

  connection->DidChangeState(state);
}

void PresentationController::OnConnectionClosed(
    mojom::blink::PresentationInfoPtr presentation_info,
    mojom::blink::PresentationConnectionCloseReason reason,
    const String& message) {
  PresentationConnection* connection = FindConnection(*presentation_info);
  if (!connection)
    return;

  connection->DidClose(reason, message);
}

void PresentationController::OnDefaultPresentationStarted(
    mojom::blink::PresentationConnectionResultPtr result) {
  DCHECK(result);
  DCHECK(result->presentation_info);
  DCHECK(result->connection_remote && result->connection_receiver);
  if (!presentation_ || !presentation_->defaultRequest())
    return;

  auto* connection = ControllerPresentationConnection::Take(
      this, *result->presentation_info, presentation_->defaultRequest());
  // TODO(btolsch): Convert this and similar calls to just use InterfacePtrInfo
  // instead of constructing an InterfacePtr every time we have
  // InterfacePtrInfo.
  connection->Init(std::move(result->connection_remote),
                   std::move(result->connection_receiver));
}

ControllerPresentationConnection*
PresentationController::FindExistingConnection(
    const blink::WebVector<blink::WebURL>& presentation_urls,
    const blink::WebString& presentation_id) {
  for (const auto& connection : connections_) {
    for (const auto& presentation_url : presentation_urls) {
      if (connection->GetState() !=
              mojom::blink::PresentationConnectionState::TERMINATED &&
          connection->Matches(presentation_id, presentation_url)) {
        return connection.Get();
      }
    }
  }
  return nullptr;
}

HeapMojoRemote<mojom::blink::PresentationService>&
PresentationController::GetPresentationService() {
  if (!presentation_service_remote_ && GetSupplementable()) {
    scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        GetSupplementable()->GetTaskRunner(TaskType::kPresentation);
    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        presentation_service_remote_.BindNewPipeAndPassReceiver(task_runner));

    // Note: `presentation_controller_receiver_` should always be unbound in
    // production. But sometimes it might be bound during tests, as it means the
    // controller remote was unbound, the controller receiver remains bound and
    // the controller hasn't been GCed.
    if (!presentation_controller_receiver_.is_bound()) {
      presentation_service_remote_->SetController(
          presentation_controller_receiver_.BindNewPipeAndPassRemote(
              task_runner));
    }
  }
  return presentation_service_remote_;
}

ControllerPresentationConnection* PresentationController::FindConnection(
    const mojom::blink::PresentationInfo& presentation_info) const {
  for (const auto& connection : connections_) {
    if (connection->Matches(presentation_info.id, presentation_info.url))
      return connection.Get();
  }

  return nullptr;
}

}  // namespace blink
