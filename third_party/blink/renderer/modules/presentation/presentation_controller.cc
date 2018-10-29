// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"

#include <memory>
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_callbacks.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_observer.h"
#include "third_party/blink/renderer/modules/presentation/presentation_availability_state.h"
#include "third_party/blink/renderer/modules/presentation/presentation_connection.h"

namespace blink {

PresentationController::PresentationController(LocalFrame& frame)
    : Supplement<LocalFrame>(frame),
      ContextLifecycleObserver(frame.GetDocument()),
      controller_binding_(this) {}

PresentationController::~PresentationController() = default;

// static
const char PresentationController::kSupplementName[] = "PresentationController";

// static
PresentationController* PresentationController::From(LocalFrame& frame) {
  return Supplement<LocalFrame>::From<PresentationController>(frame);
}

// static
void PresentationController::ProvideTo(LocalFrame& frame) {
  Supplement<LocalFrame>::ProvideTo(frame, new PresentationController(frame));
}

// static
PresentationController* PresentationController::FromContext(
    ExecutionContext* execution_context) {
  if (!execution_context)
    return nullptr;

  Document* document = To<Document>(execution_context);
  if (!document->GetFrame())
    return nullptr;

  return PresentationController::From(*document->GetFrame());
}

void PresentationController::Trace(blink::Visitor* visitor) {
  visitor->Trace(presentation_);
  visitor->Trace(connections_);
  Supplement<LocalFrame>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
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
    availability_state_.reset(
        new PresentationAvailabilityState(GetPresentationService().get()));
  }

  return availability_state_.get();
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
  DCHECK(result->connection_ptr && result->connection_request);
  if (!presentation_ || !presentation_->defaultRequest())
    return;

  PresentationRequest::RecordStartOriginTypeAccess(*GetExecutionContext());
  auto* connection = ControllerPresentationConnection::Take(
      this, *result->presentation_info, presentation_->defaultRequest());
  // TODO(btolsch): Convert this and similar calls to just use InterfacePtrInfo
  // instead of constructing an InterfacePtr every time we have
  // InterfacePtrInfo.
  connection->Init(mojom::blink::PresentationConnectionPtr(
                       std::move(result->connection_ptr)),
                   std::move(result->connection_request));
}

void PresentationController::ContextDestroyed(ExecutionContext*) {
  controller_binding_.Close();
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

mojom::blink::PresentationServicePtr&
PresentationController::GetPresentationService() {
  if (!presentation_service_ && GetFrame() && GetFrame()->Client()) {
    auto* interface_provider = GetFrame()->Client()->GetInterfaceProvider();
    interface_provider->GetInterface(mojo::MakeRequest(&presentation_service_));

    mojom::blink::PresentationControllerPtr controller_ptr;
    controller_binding_.Bind(mojo::MakeRequest(&controller_ptr));
    presentation_service_->SetController(std::move(controller_ptr));
  }
  return presentation_service_;
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
