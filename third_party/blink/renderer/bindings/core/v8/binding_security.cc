/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/image_document.h"
#include "third_party/blink/renderer/core/html/media/media_document.h"
#include "third_party/blink/renderer/core/html/text_document.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

// Documents that have the same WindowAgentFactory should be able to
// share data with each other if they have the same Agent and are
// SameOriginDomain.
bool IsSameWindowAgentFactory(const LocalDOMWindow* window1,
                              const LocalDOMWindow* window2) {
  return window1->GetFrame() && window2->GetFrame() &&
         &window1->GetFrame()->window_agent_factory() ==
             &window2->GetFrame()->window_agent_factory();
}

}  // namespace

void BindingSecurity::Init() {
  BindingSecurityForPlatform::SetShouldAllowAccessToV8Context(
      ShouldAllowAccessToV8Context);
}

namespace {

void ThrowSecurityError(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target_window,
    DOMWindow::CrossDocumentAccessPolicy cross_document_access,
    ExceptionState* exception_state) {
  if (!exception_state) {
    return;
  }
  if (target_window) {
    exception_state->ThrowSecurityError(
        target_window->SanitizedCrossDomainAccessErrorMessage(
            accessing_window, cross_document_access),
        target_window->CrossDomainAccessErrorMessage(accessing_window,
                                                     cross_document_access));
  } else {
    exception_state->ThrowSecurityError("Cross origin access was denied.");
  }
}

bool CanAccessWindowInternal(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target_window,
    DOMWindow::CrossDocumentAccessPolicy* cross_document_access) {
  SECURITY_CHECK(!(target_window && target_window->GetFrame()) ||
                 target_window == target_window->GetFrame()->DomWindow());
  DCHECK_EQ(DOMWindow::CrossDocumentAccessPolicy::kAllowed,
            *cross_document_access);

  // It's important to check that target_window is a LocalDOMWindow: it's
  // possible for a remote frame and local frame to have the same security
  // origin, depending on the model being used to allocate Frames between
  // processes. See https://crbug.com/601629.
  const auto* local_target_window = DynamicTo<LocalDOMWindow>(target_window);
  if (!(accessing_window && local_target_window))
    return false;

  const SecurityOrigin* accessing_origin =
      accessing_window->GetSecurityOrigin();

  SecurityOrigin::AccessResultDomainDetail detail;
  bool can_access = accessing_origin->CanAccess(
      local_target_window->GetSecurityOrigin(), detail);
  if (detail ==
          SecurityOrigin::AccessResultDomainDetail::kDomainSetByOnlyOneOrigin ||
      detail ==
          SecurityOrigin::AccessResultDomainDetail::kDomainMatchNecessary ||
      detail == SecurityOrigin::AccessResultDomainDetail::kDomainMismatch) {
    UseCounter::Count(
        accessing_window->document(),
        can_access ? WebFeature::kDocumentDomainEnabledCrossOriginAccess
                   : WebFeature::kDocumentDomainBlockedCrossOriginAccess);
  }
  if (!can_access) {
    // Ensure that if we got a cluster mismatch that it was due to a permissions
    // policy being enabled and not a logic bug.
    if (detail == SecurityOrigin::AccessResultDomainDetail::
                      kDomainNotRelevantAgentClusterMismatch) {
      // Assert that because the agent clusters are different than the
      // WindowAgentFactories must also be different unless they differ in
      // being explicitly origin keyed.
      SECURITY_CHECK(
          !IsSameWindowAgentFactory(accessing_window, local_target_window) ||
          (accessing_window->GetAgent()->IsOriginKeyedForInheritance() !=
           local_target_window->GetAgent()->IsOriginKeyedForInheritance()) ||
          (WebTestSupport::IsRunningWebTest() &&
           local_target_window->GetFrame()->PagePopupOwner()));

      *cross_document_access =
          DOMWindow::CrossDocumentAccessPolicy::kDisallowed;
    }
    return false;
  }

  if (accessing_window != local_target_window) {
    Document* doc = local_target_window->document();
    if (doc->IsImageDocument() || doc->IsMediaDocument() ||
        doc->IsTextDocument() ||
        (doc->IsXMLDocument() && !doc->IsXHTMLDocument() &&
         !doc->IsSVGDocument())) {
      UseCounter::Count(
          accessing_window->document(),
          WebFeature::kCrossWindowAccessToBrowserGeneratedDocument);
    }
  }

  // Notify the loader's client if the initial document has been accessed.
  LocalFrame* target_frame = local_target_window->GetFrame();
  if (target_frame && target_frame->GetDocument()->IsInitialEmptyDocument()) {
    target_frame->Loader().DidAccessInitialDocument();
  }

  return true;
}

bool CanAccessWindow(const LocalDOMWindow* accessing_window,
                     const DOMWindow* target_window,
                     ExceptionState* exception_state) {
  DOMWindow::CrossDocumentAccessPolicy cross_document_access =
      DOMWindow::CrossDocumentAccessPolicy::kAllowed;
  if (CanAccessWindowInternal(accessing_window, target_window,
                              &cross_document_access)) {
    return true;
  }

  ThrowSecurityError(accessing_window, target_window, cross_document_access,
                     exception_state);
  return false;
}

DOMWindow* FindWindow(v8::Isolate* isolate,
                      const WrapperTypeInfo* type,
                      v8::Local<v8::Object> holder) {
  if (V8Window::GetWrapperTypeInfo()->Equals(type))
    return V8Window::ToWrappableUnsafe(isolate, holder);

  if (V8Location::GetWrapperTypeInfo()->Equals(type))
    return V8Location::ToWrappableUnsafe(isolate, holder)->DomWindow();

  // This function can handle only those types listed above.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target) {
  DCHECK(target);
  bool can_access = CanAccessWindow(accessing_window, target, nullptr);

  if (!can_access && accessing_window) {
    UseCounter::Count(accessing_window->document(),
                      WebFeature::kCrossOriginPropertyAccess);
    if (target->opener() == accessing_window) {
      UseCounter::Count(accessing_window->document(),
                        WebFeature::kCrossOriginPropertyAccessFromOpener);
    }
  }

  return can_access;
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Location* target) {
  DCHECK(target);
  bool can_access =
      CanAccessWindow(accessing_window, target->DomWindow(), nullptr);

  if (!can_access && accessing_window) {
    UseCounter::Count(accessing_window->document(),
                      WebFeature::kCrossOriginPropertyAccess);
    if (target->DomWindow()->opener() == accessing_window) {
      UseCounter::Count(accessing_window->document(),
                        WebFeature::kCrossOriginPropertyAccessFromOpener);
    }
  }

  return can_access;
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Node* target) {
  if (!target)
    return false;
  return CanAccessWindow(accessing_window, target->GetDocument().domWindow(),
                         nullptr);
}

bool BindingSecurity::ShouldAllowAccessToV8ContextInternal(
    ScriptState* accessing_script_state,
    ScriptState* target_script_state,
    ExceptionState* exception_state) {
  // Workers and worklets do not support multiple contexts, so both of
  // |accessing_context| and |target_context| must be windows at this point.

  const DOMWrapperWorld& accessing_world = accessing_script_state->World();
  const DOMWrapperWorld& target_world = target_script_state->World();
  CHECK_EQ(accessing_world.GetWorldId(), target_world.GetWorldId());
  return !accessing_world.IsMainWorld() ||
         CanAccessWindow(ToLocalDOMWindow(accessing_script_state),
                         ToLocalDOMWindow(target_script_state),
                         exception_state);
}

bool BindingSecurity::ShouldAllowAccessToV8Context(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> maybe_target_context) {
  ExceptionState* exception_state = nullptr;

  // remote_object->GetCreationContext() returns the empty handle. Remote
  // contexts are unconditionally treated as cross origin.
  v8::Local<v8::Context> target_context;
  if (!maybe_target_context.ToLocal(&target_context)) {
    ThrowSecurityError(ToLocalDOMWindow(accessing_context), nullptr,
                       DOMWindow::CrossDocumentAccessPolicy::kAllowed,
                       exception_state);
    return false;
  }

  // Fast path for the most likely case.
  if (accessing_context == target_context) [[likely]] {
    return true;
  }

  v8::Isolate* isolate = accessing_context->GetIsolate();
  return ShouldAllowAccessToV8ContextInternal(
      ScriptState::From(isolate, accessing_context),
      ScriptState::From(isolate, target_context), exception_state);
}

void BindingSecurity::FailedAccessCheckFor(v8::Isolate* isolate,
                                           const WrapperTypeInfo* type,
                                           v8::Local<v8::Object> holder,
                                           ExceptionState& exception_state) {
  DOMWindow* target = FindWindow(isolate, type, holder);
  // Failing to find a target means something is wrong. Failing to throw an
  // exception could be a security issue, so just crash.
  CHECK(target);

  auto* local_dom_window = CurrentDOMWindow(isolate);
  // Determine if the access check failure was because of cross-origin or if the
  // WindowAgentFactory is different. If the WindowAgentFactories are different
  // so report the error as "restricted" instead of "cross-origin".
  DOMWindow::CrossDocumentAccessPolicy cross_document_access =
      (!target->ToLocalDOMWindow() ||
       IsSameWindowAgentFactory(local_dom_window, target->ToLocalDOMWindow()))
          ? DOMWindow::CrossDocumentAccessPolicy::kAllowed
          : DOMWindow::CrossDocumentAccessPolicy::kDisallowed;
  exception_state.ThrowSecurityError(
      target->SanitizedCrossDomainAccessErrorMessage(local_dom_window,
                                                     cross_document_access),
      target->CrossDomainAccessErrorMessage(local_dom_window,
                                            cross_document_access));
}

}  // namespace blink
