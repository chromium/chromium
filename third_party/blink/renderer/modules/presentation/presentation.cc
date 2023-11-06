// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/presentation/presentation_controller.h"
#include "third_party/blink/renderer/modules/presentation/presentation_receiver.h"
#include "third_party/blink/renderer/modules/presentation/presentation_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Checks if the frame of the provided window is the outermost frame, which
// means, neither an iframe, or a fenced frame.
bool IsOutermostDocument(LocalDOMWindow* window) {
  return window->GetFrame()->IsMainFrame() &&
         !window->GetFrame()->IsInFencedFrameTree();
}

}  // namespace

// static
const char Presentation::kSupplementName[] = "Presentation";

// static
Presentation* Presentation::presentation(Navigator& navigator) {
  if (!navigator.DomWindow())
    return nullptr;
  auto* presentation = Supplement<Navigator>::From<Presentation>(navigator);
  if (!presentation) {
    presentation = MakeGarbageCollected<Presentation>(navigator);
    ProvideTo(navigator, presentation);
  }
  return presentation;
}

Presentation::Presentation(Navigator& navigator)
    : Supplement<Navigator>(navigator) {
  PresentationController::From(*navigator.DomWindow())->SetPresentation(this);
  MaybeInitReceiver();
}

void Presentation::Trace(Visitor* visitor) const {
  visitor->Trace(default_request_);
  visitor->Trace(receiver_);
  ScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);
}

PresentationRequest* Presentation::defaultRequest() const {
  return default_request_.Get();
}

void Presentation::setDefaultRequest(PresentationRequest* request) {
  default_request_ = request;

  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!window)
    return;

  PresentationController* controller = PresentationController::From(*window);
  controller->GetPresentationService()->SetDefaultPresentationUrls(
      request ? request->Urls() : WTF::Vector<KURL>());
}

void Presentation::MaybeInitReceiver() {
  LocalDOMWindow* window = GetSupplementable()->DomWindow();
  if (!receiver_ && window && IsOutermostDocument(window) &&
      window->GetFrame()->GetSettings()->GetPresentationReceiver()) {
    receiver_ = MakeGarbageCollected<PresentationReceiver>(window);
  }
}

PresentationReceiver* Presentation::receiver() {
  MaybeInitReceiver();
  return receiver_.Get();
}

}  // namespace blink
