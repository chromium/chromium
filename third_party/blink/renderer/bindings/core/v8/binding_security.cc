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
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
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
  BindingSecurityForPlatform::SetShouldAllowAccessToV8ContextWithExceptionState(
      ShouldAllowAccessToV8Context);
  BindingSecurityForPlatform::
      SetShouldAllowAccessToV8ContextWithErrorReportOption(
          ShouldAllowAccessToV8Context);
  BindingSecurityForPlatform::SetShouldAllowWrapperCreationOrThrowException(
      ShouldAllowWrapperCreationOrThrowException);
  BindingSecurityForPlatform::SetRethrowWrapperCreationException(
      RethrowWrapperCreationException);
}

namespace {

void ReportOrThrowSecurityError(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target_window,
    DOMWindow::CrossDocumentAccessPolicy cross_document_access,
    ExceptionState& exception_state) {
  if (target_window) {
    exception_state.ThrowSecurityError(
        target_window->SanitizedCrossDomainAccessErrorMessage(
            accessing_window, cross_document_access),
        target_window->CrossDomainAccessErrorMessage(accessing_window,
                                                     cross_document_access));
  } else {
    exception_state.ThrowSecurityError("Cross origin access was denied.");
  }
}

void ReportOrThrowSecurityError(
    const LocalDOMWindow* accessing_window,
    const DOMWindow* target_window,
    DOMWindow::CrossDocumentAccessPolicy cross_document_access,
    BindingSecurity::ErrorReportOption reporting_option) {
  if (reporting_option == BindingSecurity::ErrorReportOption::kDoNotReport)
    return;

  if (accessing_window && target_window) {
    accessing_window->PrintErrorMessage(
        target_window->CrossDomainAccessErrorMessage(accessing_window,
                                                     cross_document_access));
  } else if (accessing_window) {
    accessing_window->PrintErrorMessage("Cross origin access was denied.");
  } else {
    // Nowhere to report the error.
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
    // Handle deprecation warnings for OriginAgentCluster default:
    // If the new default is not (yet) enabled, but warnings are, and
    // access gets allowed for domain-setting reasons (reasons checked in
    // the if clause above).
    if (accessing_window->GetAgent()->IsOriginOrSiteKeyedBasedOnDefault() &&
        base::FeatureList::IsEnabled(
            blink::features::kOriginAgentClusterDefaultWarning) &&
        can_access) {
      UseCounter::CountDeprecation(
          accessing_window->document(),
          WebFeature::kCrossOriginAccessBasedOnDocumentDomain);
    }
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

  // Notify the loader's client if the initial document has been accessed.
  LocalFrame* target_frame = local_target_window->GetFrame();
  if (target_frame && target_frame->GetDocument()->IsInitialEmptyDocument()) {
    target_frame->Loader().DidAccessInitialDocument();
  }

  return true;
}

template <typename ExceptionStateOrErrorReportOption>
bool CanAccessWindow(const LocalDOMWindow* accessing_window,
                     const DOMWindow* target_window,
                     ExceptionStateOrErrorReportOption& error_report) {
  DOMWindow::CrossDocumentAccessPolicy cross_document_access =
      DOMWindow::CrossDocumentAccessPolicy::kAllowed;
  if (CanAccessWindowInternal(accessing_window, target_window,
                              &cross_document_access))
    return true;

  ReportOrThrowSecurityError(accessing_window, target_window,
                             cross_document_access, error_report);
  return false;
}

DOMWindow* FindWindow(v8::Isolate* isolate,
                      const WrapperTypeInfo* type,
                      v8::Local<v8::Object> holder) {
  if (V8Window::GetWrapperTypeInfo()->Equals(type))
    return V8Window::ToImpl(holder);

  if (V8Location::GetWrapperTypeInfo()->Equals(type))
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

namespace {

template <typename ExceptionStateOrErrorReportOption>
bool ShouldAllowAccessToV8ContextInternal(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> maybe_target_context,
    ExceptionStateOrErrorReportOption& error_report) {
  // Workers and worklets do not support multiple contexts, so both of
  // |accessing_context| and |target_context| must be windows at this point.

  // remote_object->GetCreationContext() returns the empty handle. Remote
  // contexts are unconditionally treated as cross origin.
  v8::Local<v8::Context> target_context;
  if (!maybe_target_context.ToLocal(&target_context)) {
    ReportOrThrowSecurityError(ToLocalDOMWindow(accessing_context), nullptr,
                               DOMWindow::CrossDocumentAccessPolicy::kAllowed,
                               error_report);
    return false;
  }
  // Fast path for the most likely case.
  if (accessing_context == target_context)
    return true;

  LocalFrame* target_frame = ToLocalFrameIfNotDetached(target_context);
  // TODO(dcheng): Why doesn't this code just use DOMWindows throughout? Can't
  // we just always use ToLocalDOMWindow(context)?
  if (!target_frame) {
    // Sandbox detached frames - they can't create cross origin objects.
    LocalDOMWindow* accessing_window = ToLocalDOMWindow(accessing_context);
    LocalDOMWindow* target_window = ToLocalDOMWindow(target_context);

    // TODO(https://crbug.com/723057): This is tricky: this intentionally uses
    // the internal CanAccessWindow() helper rather than ShouldAllowAccessTo().
    // ShouldAllowAccessTo() unconditionally denies access if the DOMWindow is
    // not attached to a Frame, but this code is intended for handling the
    // detached DOMWindow case.
    return CanAccessWindow(accessing_window, target_window, error_report);
  }

  const DOMWrapperWorld& accessing_world =
      DOMWrapperWorld::World(accessing_context);
  const DOMWrapperWorld& target_world = DOMWrapperWorld::World(target_context);
  CHECK_EQ(accessing_world.GetWorldId(), target_world.GetWorldId());

  return !accessing_world.IsMainWorld() ||
         BindingSecurity::ShouldAllowAccessToFrame(
             ToLocalDOMWindow(accessing_context), target_frame, error_report);
}

}  // namespace

bool BindingSecurity::ShouldAllowAccessToV8Context(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> target_context,
    ExceptionState& exception_state) {
  return ShouldAllowAccessToV8ContextInternal(accessing_context, target_context,
                                              exception_state);
}

bool BindingSecurity::ShouldAllowAccessToV8Context(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> target_context,
    ErrorReportOption reporting_option) {
  return ShouldAllowAccessToV8ContextInternal(accessing_context, target_context,
                                              reporting_option);
}

bool BindingSecurity::ShouldAllowWrapperCreationOrThrowException(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> creation_context,
    const WrapperTypeInfo* wrapper_type_info) {
  // Fast path for the most likely case.
  if (!creation_context.IsEmpty() &&
      accessing_context == creation_context.ToLocalChecked())
    return true;

  // According to
  // https://html.spec.whatwg.org/C/#security-location,
  // cross-origin script access to a few properties of Location is allowed.
  // Location already implements the necessary security checks.
  if (wrapper_type_info->Equals(V8Location::GetWrapperTypeInfo()))
    return true;

  ExceptionState exception_state(accessing_context->GetIsolate(),
                                 ExceptionState::kConstructionContext,
                                 wrapper_type_info->interface_name);
  return ShouldAllowAccessToV8Context(accessing_context, creation_context,
                                      exception_state);
}

void BindingSecurity::RethrowWrapperCreationException(
    v8::Local<v8::Context> accessing_context,
    v8::MaybeLocal<v8::Context> creation_context,
    const WrapperTypeInfo* wrapper_type_info,
    v8::Local<v8::Value> cross_context_exception) {
  DCHECK(!cross_context_exception.IsEmpty());
  v8::Isolate* isolate = creation_context.ToLocalChecked()->GetIsolate();
  ExceptionState exception_state(isolate, ExceptionState::kConstructionContext,
                                 wrapper_type_info->interface_name);
  if (!ShouldAllowAccessToV8Context(accessing_context, creation_context,
                                    exception_state)) {
    // A cross origin exception has turned into a SecurityError.
    CHECK(exception_state.HadException());
    return;
  }
  exception_state.RethrowV8Exception(cross_context_exception);
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

  auto* local_dom_window = CurrentDOMWindow(isolate);
  // Determine if the access check failure was because of cross-origin or if the
  // WindowAgentFactory is different. If the WindowAgentFactories are different
  // so report the error as "restricted" instead of "cross-origin".
  DOMWindow::CrossDocumentAccessPolicy cross_document_access =
      (!target->ToLocalDOMWindow() ||
       IsSameWindowAgentFactory(local_dom_window, target->ToLocalDOMWindow()))
          ? DOMWindow::CrossDocumentAccessPolicy::kAllowed
          : DOMWindow::CrossDocumentAccessPolicy::kDisallowed;

  // TODO(dcheng): Add ContextType, interface name, and property name as
  // arguments, so the generated exception can be more descriptive.
  ExceptionState exception_state(isolate, ExceptionState::kUnknownContext,
                                 nullptr, nullptr);
  exception_state.ThrowSecurityError(
      target->SanitizedCrossDomainAccessErrorMessage(local_dom_window,
                                                     cross_document_access),
      target->CrossDomainAccessErrorMessage(local_dom_window,
                                            cross_document_access));
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

}  // namespace blink
