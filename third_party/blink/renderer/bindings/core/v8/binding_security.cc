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

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_creation_security_check.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

bool CanAccessWindowInternal(const LocalDOMWindow* accessing_window,
                             const DOMWindow* target_window) {
  SECURITY_CHECK(!(target_window && target_window->GetFrame()) ||
                 target_window == target_window->GetFrame()->DomWindow());

  // It's important to check that target_window is a LocalDOMWindow: it's
  // possible for a remote frame and local frame to have the same security
  // origin, depending on the model being used to allocate Frames between
  // processes. See https://crbug.com/601629.
  if (!(accessing_window && target_window && target_window->IsLocalDOMWindow()))
    return false;

  const SecurityOrigin* accessing_origin =
      accessing_window->document()->GetSecurityOrigin();
  const LocalDOMWindow* local_target_window = ToLocalDOMWindow(target_window);

  SecurityOrigin::AccessResultDomainDetail detail;
  bool can_access = accessing_origin->CanAccess(
      local_target_window->document()->GetSecurityOrigin(), detail);
  if (detail ==
          SecurityOrigin::AccessResultDomainDetail::kDomainSetByOnlyOneOrigin ||
      detail ==
          SecurityOrigin::AccessResultDomainDetail::kDomainMatchNecessary ||
      detail == SecurityOrigin::AccessResultDomainDetail::kDomainMismatch) {
    UseCounter::Count(
        accessing_window->GetFrame(),
        can_access ? WebFeature::kDocumentDomainEnabledCrossOriginAccess
                   : WebFeature::kDocumentDomainBlockedCrossOriginAccess);
  }
  if (!can_access)
    return false;

  // Notify the loader's client if the initial document has been accessed.
  LocalFrame* target_frame = local_target_window->GetFrame();
  if (target_frame &&
      target_frame->Loader().StateMachine()->IsDisplayingInitialEmptyDocument())
    target_frame->Loader().DidAccessInitialDocument();

  return true;
}

bool CanAccessWindow(const LocalDOMWindow* accessing_window,
                     const DOMWindow* target_window,
                     ExceptionState& exception_state) {
  if (CanAccessWindowInternal(accessing_window, target_window))
    return true;

  if (target_window)
    exception_state.ThrowSecurityError(
        target_window->SanitizedCrossDomainAccessErrorMessage(accessing_window),
        target_window->CrossDomainAccessErrorMessage(accessing_window));
  return false;
}

bool CanAccessWindow(const LocalDOMWindow* accessing_window,
                     const DOMWindow* target_window,
                     BindingSecurity::ErrorReportOption reporting_option) {
  if (CanAccessWindowInternal(accessing_window, target_window))
    return true;

  if (accessing_window && target_window &&
      reporting_option == BindingSecurity::ErrorReportOption::kReport)
    accessing_window->PrintErrorMessage(
        target_window->CrossDomainAccessErrorMessage(accessing_window));
  return false;
}

DOMWindow* FindWindow(v8::Isolate* isolate,
                      const WrapperTypeInfo* type,
                      v8::Local<v8::Object> holder) {
  if (V8Window::wrapperTypeInfo.Equals(type))
    return V8Window::ToImpl(holder);

  if (V8Location::wrapperTypeInfo.Equals(type))
    return V8Location::ToImpl(holder)->DomWindow();

  // This function can handle only those types listed above.
  NOTREACHED();
  return nullptr;
}

}  // namespace

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target,
    ExceptionState& exception_state) {
  DCHECK(target);

  // TODO(https://crbug.com/723057): This is intended to match the legacy
  // behavior of when access checks revolved around Frame pointers rather than
  // DOMWindow pointers. This prevents web-visible behavior changes, since the
  // previous implementation had to follow the back pointer to the Frame, and
  // would have to early return when it was null.
  if (!target->GetFrame())
    return false;
  bool can_access = CanAccessWindow(accessing_window, target, exception_state);

  if (!can_access) {
    UseCounter::Count(accessing_window->GetFrame(),
                      WebFeature::kCrossOriginPropertyAccess);
    if (target->opener() == accessing_window) {
      UseCounter::Count(accessing_window->GetFrame(),
                        WebFeature::kCrossOriginPropertyAccessFromOpener);
    }
  }

  return can_access;
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target,
    ErrorReportOption reporting_option) {
  DCHECK(target);

  // TODO(https://crbug.com/723057): This is intended to match the legacy
  // behavior of when access checks revolved around Frame pointers rather than
  // DOMWindow pointers. This prevents web-visible behavior changes, since the
  // previous implementation had to follow the back pointer to the Frame, and
  // would have to early return when it was null.
  if (!target->GetFrame())
    return false;

  bool can_access = CanAccessWindow(accessing_window, target, reporting_option);

  if (!can_access) {
    UseCounter::Count(accessing_window->GetFrame(),
                      WebFeature::kCrossOriginPropertyAccess);
    if (target->opener() == accessing_window) {
      UseCounter::Count(accessing_window->GetFrame(),
                        WebFeature::kCrossOriginPropertyAccessFromOpener);
    }
  }

  return can_access;
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Location* target,
    ExceptionState& exception_state) {
  DCHECK(target);

  // TODO(https://crbug.com/723057): This is intended to match the legacy
  // behavior of when access checks revolved around Frame pointers rather than
  // DOMWindow pointers. This prevents web-visible behavior changes, since the
  // previous implementation had to follow the back pointer to the Frame, and
  // would have to early return when it was null.
  if (!target->DomWindow()->GetFrame())
    return false;

  bool can_access =
      CanAccessWindow(accessing_window, target->DomWindow(), exception_state);

  if (!can_access) {
    UseCounter::Count(accessing_window->GetFrame(),
                      WebFeature::kCrossOriginPropertyAccess);
    if (target->DomWindow()->opener() == accessing_window) {
      UseCounter::Count(accessing_window->GetFrame(),
                        WebFeature::kCrossOriginPropertyAccessFromOpener);
    }
  }

  return can_access;
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Location* target,
    ErrorReportOption reporting_option) {
  DCHECK(target);

  // TODO(https://crbug.com/723057): This is intended to match the legacy
  // behavior of when access checks revolved around Frame pointers rather than
  // DOMWindow pointers. This prevents web-visible behavior changes, since the
  // previous implementation had to follow the back pointer to the Frame, and
  // would have to early return when it was null.
  if (!target->DomWindow()->GetFrame())
    return false;

  bool can_access =
      CanAccessWindow(accessing_window, target->DomWindow(), reporting_option);

  if (!can_access) {
    UseCounter::Count(accessing_window->GetFrame(),
                      WebFeature::kCrossOriginPropertyAccess);
    if (target->DomWindow()->opener() == accessing_window) {
      UseCounter::Count(accessing_window->GetFrame(),
                        WebFeature::kCrossOriginPropertyAccessFromOpener);
    }
  }

  return can_access;
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Node* target,
    ExceptionState& exception_state) {
  if (!target)
    return false;
  return CanAccessWindow(accessing_window, target->GetDocument().domWindow(),
                         exception_state);
}

bool BindingSecurity::ShouldAllowAccessTo(
    const LocalDOMWindow* accessing_window,
    const Node* target,
    ErrorReportOption reporting_option) {
  if (!target)
    return false;
  return CanAccessWindow(accessing_window, target->GetDocument().domWindow(),
                         reporting_option);
}

bool BindingSecurity::ShouldAllowAccessToFrame(
    const LocalDOMWindow* accessing_window,
    const Frame* target,
    ExceptionState& exception_state) {
  if (!target || !target->GetSecurityContext())
    return false;
  return CanAccessWindow(accessing_window, target->DomWindow(),
                         exception_state);
}

bool BindingSecurity::ShouldAllowAccessToFrame(
    const LocalDOMWindow* accessing_window,
    const Frame* target,
    ErrorReportOption reporting_option) {
  if (!target || !target->GetSecurityContext())
    return false;
  return CanAccessWindow(accessing_window, target->DomWindow(),
                         reporting_option);
}

bool BindingSecurity::ShouldAllowNamedAccessTo(
    const DOMWindow* accessing_window,
    const DOMWindow* target_window) {
  const Frame* accessing_frame = accessing_window->GetFrame();
  DCHECK(accessing_frame);
  DCHECK(accessing_frame->GetSecurityContext());
  const SecurityOrigin* accessing_origin =
      accessing_frame->GetSecurityContext()->GetSecurityOrigin();

  const Frame* target_frame = target_window->GetFrame();
  DCHECK(target_frame);
  DCHECK(target_frame->GetSecurityContext());
  const SecurityOrigin* target_origin =
      target_frame->GetSecurityContext()->GetSecurityOrigin();
  SECURITY_CHECK(!(target_window && target_window->GetFrame()) ||
                 target_window == target_window->GetFrame()->DomWindow());

  if (!accessing_origin->CanAccess(target_origin))
    return false;

  // Note that there is no need to call back
  // FrameLoader::didAccessInitialDocument() because |targetWindow| must be
  // a child window inside iframe or frame and it doesn't have a URL bar,
  // so there is no need to worry about URL spoofing.

  return true;
}

bool BindingSecurity::ShouldAllowAccessToCreationContext(
    v8::Local<v8::Context> creation_context,
    const WrapperTypeInfo* type) {
  // According to
  // https://html.spec.whatwg.org/multipage/browsers.html#security-location,
  // cross-origin script access to a few properties of Location is allowed.
  // Location already implements the necessary security checks.
  if (type->Equals(&V8Location::wrapperTypeInfo))
    return true;

  v8::Isolate* isolate = creation_context->GetIsolate();
  LocalFrame* frame = ToLocalFrameIfNotDetached(creation_context);
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 type->interface_name);
  // TODO(dcheng): Why doesn't this code just use DOMWindows throughout? Can't
  // we just always use ToLocalDOMWindow(creation_context)?
  if (!frame) {
    // Sandbox detached frames - they can't create cross origin objects.
    LocalDOMWindow* calling_window = CurrentDOMWindow(isolate);
    LocalDOMWindow* target_window = ToLocalDOMWindow(creation_context);

    // TODO(https://crbug.com/723057): This is tricky: this intentionally uses
    // the internal CanAccessWindow() helper rather than ShouldAllowAccessTo().
    // ShouldAllowAccessTo() unconditionally denies access if the DOMWindow is
    // not attached to a Frame, but this code is intended for handling the
    // detached DOMWindow case.
    return CanAccessWindow(calling_window, target_window, exception_state);
  }
  const DOMWrapperWorld& current_world =
      DOMWrapperWorld::World(isolate->GetCurrentContext());
  CHECK_EQ(current_world.GetWorldId(),
           DOMWrapperWorld::World(creation_context).GetWorldId());

  return !current_world.IsMainWorld() ||
         ShouldAllowAccessToFrame(CurrentDOMWindow(isolate), frame,
                                  exception_state);
}

void BindingSecurity::RethrowCrossContextException(
    v8::Local<v8::Context> creation_context,
    const WrapperTypeInfo* type,
    v8::Local<v8::Value> cross_context_exception) {
  DCHECK(!cross_context_exception.IsEmpty());
  v8::Isolate* isolate = creation_context->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 type->interface_name);
  if (type->Equals(&V8Location::wrapperTypeInfo)) {
    // Convert cross-context exception to security error
    LocalDOMWindow* calling_window = CurrentDOMWindow(isolate);
    LocalDOMWindow* target_window = ToLocalDOMWindow(creation_context);
    exception_state.ThrowSecurityError(
        target_window->SanitizedCrossDomainAccessErrorMessage(calling_window),
        target_window->CrossDomainAccessErrorMessage(calling_window));
    return;
  }
  exception_state.RethrowV8Exception(cross_context_exception);
}

void BindingSecurity::InitWrapperCreationSecurityCheck() {
  WrapperCreationSecurityCheck::SetSecurityCheckFunction(
      &ShouldAllowAccessToCreationContext);
  WrapperCreationSecurityCheck::SetRethrowExceptionFunction(
      &RethrowCrossContextException);
}

void BindingSecurity::FailedAccessCheckFor(v8::Isolate* isolate,
                                           const WrapperTypeInfo* type,
                                           v8::Local<v8::Object> holder) {
  DOMWindow* target = FindWindow(isolate, type, holder);
  // Failing to find a target means something is wrong. Failing to throw an
  // exception could be a security issue, so just crash.
  CHECK(target);

  // TODO(https://crbug.com/723057): This is intended to match the legacy
  // behavior of when access checks revolved around Frame pointers rather than
  // DOMWindow pointers. This prevents web-visible behavior changes, since the
  // previous implementation had to follow the back pointer to the Frame, and
  // would have to early return when it was null.
  if (!target->GetFrame())
    return;

  // TODO(dcheng): Add ContextType, interface name, and property name as
  // arguments, so the generated exception can be more descriptive.
  ExceptionState exception_state(isolate, ExceptionState::kUnknownContext,
                                 nullptr, nullptr);
  exception_state.ThrowSecurityError(
      target->SanitizedCrossDomainAccessErrorMessage(CurrentDOMWindow(isolate)),
      target->CrossDomainAccessErrorMessage(CurrentDOMWindow(isolate)));
}

}  // namespace blink
