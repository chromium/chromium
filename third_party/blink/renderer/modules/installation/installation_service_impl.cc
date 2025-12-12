// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/installation/installation_service_impl.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

// static
const char InstallationServiceImpl::kSupplementName[] =
    "InstallationServiceImpl";

// static
InstallationServiceImpl* InstallationServiceImpl::From(LocalDOMWindow& window) {
  return Supplement<LocalDOMWindow>::From<InstallationServiceImpl>(window);
}

// static
void InstallationServiceImpl::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::InstallationService> receiver) {
  DCHECK(frame && frame->DomWindow());
  auto* service = InstallationServiceImpl::From(*frame->DomWindow());
  if (!service) {
    service = MakeGarbageCollected<InstallationServiceImpl>(
        base::PassKey<InstallationServiceImpl>(), *frame);
    Supplement<LocalDOMWindow>::ProvideTo(*frame->DomWindow(), service);
  }
  service->Bind(std::move(receiver));
}

InstallationServiceImpl::InstallationServiceImpl(
    base::PassKey<InstallationServiceImpl>,
    LocalFrame& frame)
    : Supplement<LocalDOMWindow>(*frame.DomWindow()),
      receivers_(this, frame.DomWindow()) {}

void InstallationServiceImpl::Bind(
    mojo::PendingReceiver<mojom::blink::InstallationService> receiver) {
  // See https://bit.ly/2S0zRAS for task types.
  receivers_.Add(std::move(receiver), GetSupplementable()->GetTaskRunner(
                                          TaskType::kMiscPlatformAPI));
}

void InstallationServiceImpl::Trace(Visitor* visitor) const {
  visitor->Trace(receivers_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void InstallationServiceImpl::OnInstall() {
  GetSupplementable()->DispatchEvent(
      *Event::Create(event_type_names::kAppinstalled));
}

}  // namespace blink
