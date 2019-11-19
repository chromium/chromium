// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/navigator_service_worker.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_container.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

NavigatorServiceWorker::NavigatorServiceWorker(Navigator& navigator) {}

NavigatorServiceWorker* NavigatorServiceWorker::From(Document& document) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return nullptr;

  // TODO(kouhei): Remove below after M72, since the check is now done in
  // RenderFrameImpl::CreateServiceWorkerProvider instead.
  //
  // Bail-out if we are about to be navigated away.
  // We check that DocumentLoader is attached since:
  // - This serves as the signal since the DocumentLoader is detached in
  //   FrameLoader::PrepareForCommit().
  // - Creating ServiceWorkerProvider in
  //   RenderFrameImpl::CreateServiceWorkerProvider() assumes that there is a
  //   DocumentLoader attached to the frame.
  if (!frame->Loader().GetDocumentLoader())
    return nullptr;

  LocalDOMWindow* dom_window = frame->DomWindow();
  if (!dom_window)
    return nullptr;
  Navigator& navigator = *dom_window->navigator();
  return &From(navigator);
}

NavigatorServiceWorker& NavigatorServiceWorker::From(Navigator& navigator) {
  NavigatorServiceWorker* supplement = ToNavigatorServiceWorker(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorServiceWorker>(navigator);
    ProvideTo(navigator, supplement);
  }
  if (navigator.GetFrame() && navigator.GetFrame()
                                  ->GetSecurityContext()
                                  ->GetSecurityOrigin()
                                  ->CanAccessServiceWorkers()) {
    // Ensure ServiceWorkerContainer. It can be cleared regardless of
    // |supplement|. See comments in NavigatorServiceWorker::serviceWorker() for
    // details.
    supplement->GetOrCreateContainer(navigator.GetFrame(), ASSERT_NO_EXCEPTION);
  }
  return *supplement;
}

NavigatorServiceWorker* NavigatorServiceWorker::ToNavigatorServiceWorker(
    Navigator& navigator) {
  return Supplement<Navigator>::From<NavigatorServiceWorker>(navigator);
}

const char NavigatorServiceWorker::kSupplementName[] = "NavigatorServiceWorker";

// static
ServiceWorkerContainer* NavigatorServiceWorker::serviceWorker(
    ScriptState* script_state,
    Navigator& navigator,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(!navigator.GetFrame() ||
         execution_context->GetSecurityOrigin()->CanAccess(
             navigator.GetFrame()->GetSecurityContext()->GetSecurityOrigin()));
  return NavigatorServiceWorker::From(navigator).GetOrCreateContainer(
      navigator.GetFrame(), exception_state);
}

ServiceWorkerContainer* NavigatorServiceWorker::GetOrCreateContainer(
    LocalFrame* frame,
    ExceptionState& exception_state) {
  if (!frame)
    return nullptr;

  if (!frame->GetSecurityContext()
           ->GetSecurityOrigin()
           ->CanAccessServiceWorkers()) {
    String error_message;
    if (frame->GetSecurityContext()->IsSandboxed(WebSandboxFlags::kOrigin)) {
      error_message =
          "Service worker is disabled because the context is sandboxed and "
          "lacks the 'allow-same-origin' flag.";
    } else {
      error_message =
          "Access to service workers is denied in this document origin.";
    }
    exception_state.ThrowSecurityError(error_message);
    return nullptr;
  }

  if (frame->GetSecurityContext()->GetSecurityOrigin()->IsLocal()) {
    UseCounter::Count(frame->GetDocument(),
                      WebFeature::kFileAccessedServiceWorker);
  }

  return ServiceWorkerContainer::From(
      To<Document>(frame->DomWindow()->GetExecutionContext()));
}

void NavigatorServiceWorker::Trace(blink::Visitor* visitor) {
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
