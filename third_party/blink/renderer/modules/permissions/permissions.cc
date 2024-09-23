// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permissions.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_permission_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/permissions/permission_status.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionService;

// static
const char Permissions::kSupplementName[] = "Permissions";

// static
Permissions* Permissions::permissions(NavigatorBase& navigator) {
  Permissions* supplement =
      Supplement<NavigatorBase>::From<Permissions>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<Permissions>(navigator);
    ProvideTo(navigator, supplement);
  }
  return supplement;
}

Permissions::Permissions(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      service_(navigator.GetExecutionContext()) {}

ScriptPromise<PermissionStatus> Permissions::query(
    ScriptState* script_state,
    const ScriptValue& raw_permission,
    ExceptionState& exception_state) {
  // https://www.w3.org/TR/permissions/#query-method
  // If this's relevant global object is a Window object, and if the current
  // settings object's associated Document is not fully active, return a promise
  // rejected with an "InvalidStateError" DOMException.
  auto* context = ExecutionContext::From(script_state);
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    auto* document = window->document();
    if (document && !document->IsActive()) {
      // It's impossible for Permissions.query to occur while in BFCache.
      if (document->GetPage()) {
        DCHECK(!document->GetPage()
                    ->GetPageLifecycleState()
                    ->is_in_back_forward_cache);
      }
      exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                        "The document is not active");
      return EmptyPromise();
    }
  }

  PermissionDescriptorPtr descriptor =
      ParsePermissionDescriptor(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PermissionStatus>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // If the current origin is a file scheme, it will unlikely return a
  // meaningful value because most APIs are broken on file scheme and no
  // permission prompt will be shown even if the returned permission will most
  // likely be "prompt".
  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  base::TimeTicks query_start_time;
  GetService(context)->HasPermission(
      std::move(descriptor),
      WTF::BindOnce(&Permissions::QueryTaskComplete, WrapPersistent(this),
                    WrapPersistent(resolver), std::move(descriptor_copy),
                    query_start_time));
  return promise;
}

ScriptPromise<PermissionStatus> Permissions::request(
    ScriptState* script_state,
    const ScriptValue& raw_permission,
    ExceptionState& exception_state) {
  PermissionDescriptorPtr descriptor =
      ParsePermissionDescriptor(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  ExecutionContext* context = ExecutionContext::From(script_state);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PermissionStatus>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);
  LocalFrame* frame = window ? window->GetFrame() : nullptr;

  GetService(context)->RequestPermission(
      std::move(descriptor), LocalFrame::HasTransientUserActivation(frame),
      WTF::BindOnce(&Permissions::VerifyPermissionAndReturnStatus,
                    WrapPersistent(this), WrapPersistent(resolver),
                    std::move(descriptor_copy)));
  return promise;
}

ScriptPromise<PermissionStatus> Permissions::revoke(
    ScriptState* script_state,
    const ScriptValue& raw_permission,
    ExceptionState& exception_state) {
  PermissionDescriptorPtr descriptor =
      ParsePermissionDescriptor(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<PermissionStatus>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  GetService(ExecutionContext::From(script_state))
      ->RevokePermission(
          std::move(descriptor),
          WTF::BindOnce(&Permissions::TaskComplete, WrapPersistent(this),
                        WrapPersistent(resolver), std::move(descriptor_copy)));
  return promise;
}

ScriptPromise<IDLSequence<PermissionStatus>> Permissions::requestAll(
    ScriptState* script_state,
    const HeapVector<ScriptValue>& raw_permissions,
    ExceptionState& exception_state) {
  Vector<PermissionDescriptorPtr> internal_permissions;
  Vector<int> caller_index_to_internal_index;
  caller_index_to_internal_index.resize(raw_permissions.size());

  ExecutionContext* context = ExecutionContext::From(script_state);

  for (wtf_size_t i = 0; i < raw_permissions.size(); ++i) {
    const ScriptValue& raw_permission = raw_permissions[i];

    auto descriptor = ParsePermissionDescriptor(script_state, raw_permission,
                                                exception_state);
    if (exception_state.HadException())
      return ScriptPromise<IDLSequence<PermissionStatus>>();

    // Only append permissions types that are not already present in the vector.
    wtf_size_t internal_index = kNotFound;
    for (wtf_size_t j = 0; j < internal_permissions.size(); ++j) {
      if (internal_permissions[j]->name == descriptor->name) {
        internal_index = j;
        break;
      }
    }
    if (internal_index == kNotFound) {
      internal_index = internal_permissions.size();
      internal_permissions.push_back(std::move(descriptor));
    }
    caller_index_to_internal_index[i] = internal_index;
  }

  auto* resolver = MakeGarbageCollected<
      ScriptPromiseResolver<IDLSequence<PermissionStatus>>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  Vector<PermissionDescriptorPtr> internal_permissions_copy;
  internal_permissions_copy.reserve(internal_permissions.size());
  for (const auto& descriptor : internal_permissions)
    internal_permissions_copy.push_back(descriptor->Clone());

  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);
  LocalFrame* frame = window ? window->GetFrame() : nullptr;

  GetService(context)->RequestPermissions(
      std::move(internal_permissions),
      LocalFrame::HasTransientUserActivation(frame),
      WTF::BindOnce(
          &Permissions::VerifyPermissionsAndReturnStatus, WrapPersistent(this),
          WrapPersistent(resolver), std::move(internal_permissions_copy),
          std::move(caller_index_to_internal_index),
          -1 /* last_verified_permission_index */, true /* is_bulk_request */));
  return promise;
}

void Permissions::ContextDestroyed() {
  base::UmaHistogramCounts1000("Permissions.API.CreatedPermissionStatusObjects",
                               created_permission_status_objects_);
}

void Permissions::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  visitor->Trace(listeners_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

PermissionService* Permissions::GetService(
    ExecutionContext* execution_context) {
  if (!service_.is_bound()) {
    ConnectToPermissionService(
        execution_context,
        service_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kPermission)));
    service_.set_disconnect_handler(WTF::BindOnce(
        &Permissions::ServiceConnectionError, WrapWeakPersistent(this)));
  }
  return service_.get();
}

void Permissions::ServiceConnectionError() {
  service_.reset();
}
void Permissions::QueryTaskComplete(
    ScriptPromiseResolver<PermissionStatus>* resolver,
    mojom::blink::PermissionDescriptorPtr descriptor,
    base::TimeTicks query_start_time,
    mojom::blink::PermissionStatus result) {
  base::UmaHistogramTimes("Permissions.Query.QueryResponseTime",
                          base::TimeTicks::Now() - query_start_time);
  TaskComplete(resolver, std::move(descriptor), result);
}

void Permissions::TaskComplete(
    ScriptPromiseResolver<PermissionStatus>* resolver,
    mojom::blink::PermissionDescriptorPtr descriptor,
    mojom::blink::PermissionStatus result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  PermissionStatusListener* listener =
      GetOrCreatePermissionStatusListener(result, std::move(descriptor));
  if (listener)
    resolver->Resolve(PermissionStatus::Take(listener, resolver));
}

void Permissions::VerifyPermissionAndReturnStatus(
    ScriptPromiseResolverBase* resolver,
    mojom::blink::PermissionDescriptorPtr descriptor,
    mojom::blink::PermissionStatus result) {
  Vector<int> caller_index_to_internal_index;
  caller_index_to_internal_index.push_back(0);
  Vector<mojom::blink::PermissionStatus> results;
  results.push_back(std::move(result));
  Vector<mojom::blink::PermissionDescriptorPtr> descriptors;
  descriptors.push_back(std::move(descriptor));

  VerifyPermissionsAndReturnStatus(resolver, std::move(descriptors),
                                   std::move(caller_index_to_internal_index),
                                   -1 /* last_verified_permission_index */,
                                   false /* is_bulk_request */,
                                   std::move(results));
}

void Permissions::VerifyPermissionsAndReturnStatus(
    ScriptPromiseResolverBase* resolver,
    Vector<mojom::blink::PermissionDescriptorPtr> descriptors,
    Vector<int> caller_index_to_internal_index,
    int last_verified_permission_index,
    bool is_bulk_request,
    const Vector<mojom::blink::PermissionStatus>& results) {
  DCHECK(caller_index_to_internal_index.size() == 1u || is_bulk_request);
  DCHECK_EQ(descriptors.size(), caller_index_to_internal_index.size());

  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // Create the response vector by finding the status for each index by
  // using the caller to internal index mapping and looking up the status
  // using the internal index obtained.
  HeapVector<Member<PermissionStatus>> result;
  result.ReserveInitialCapacity(caller_index_to_internal_index.size());
  for (int internal_index : caller_index_to_internal_index) {
    // If there is a chance that this permission result came from a different
    // permission type (e.g. a PTZ request could be replaced with a camera
    // request internally), then re-check the actual permission type to ensure
    // that it it indeed that permission type. If it's not, replace the
    // descriptor with the verification descriptor.
    auto verification_descriptor = CreatePermissionVerificationDescriptor(
        *GetPermissionType(*descriptors[internal_index]));
    if (last_verified_permission_index == -1 && verification_descriptor) {
      auto descriptor_copy = descriptors[internal_index]->Clone();
      service_->HasPermission(
          std::move(descriptor_copy),
          WTF::BindOnce(&Permissions::PermissionVerificationComplete,
                        WrapPersistent(this), WrapPersistent(resolver),
                        std::move(descriptors),
                        std::move(caller_index_to_internal_index),
                        std::move(results), std::move(verification_descriptor),
                        internal_index, is_bulk_request));
      return;
    }

    // This is the last permission that was verified.
    if (internal_index == last_verified_permission_index)
      last_verified_permission_index = -1;

    PermissionStatusListener* listener = GetOrCreatePermissionStatusListener(
        results[internal_index], descriptors[internal_index]->Clone());
    if (listener) {
      // If it's not a bulk request, return the first (and only) result.
      if (!is_bulk_request) {
        resolver->DowncastTo<PermissionStatus>()->Resolve(
            PermissionStatus::Take(listener, resolver));
        return;
      }
      result.push_back(PermissionStatus::Take(listener, resolver));
    }
  }
  resolver->DowncastTo<IDLSequence<PermissionStatus>>()->Resolve(result);
}

void Permissions::PermissionVerificationComplete(
    ScriptPromiseResolverBase* resolver,
    Vector<mojom::blink::PermissionDescriptorPtr> descriptors,
    Vector<int> caller_index_to_internal_index,
    const Vector<mojom::blink::PermissionStatus>& results,
    mojom::blink::PermissionDescriptorPtr verification_descriptor,
    int internal_index_to_verify,
    bool is_bulk_request,
    mojom::blink::PermissionStatus verification_result) {
  if (verification_result != results[internal_index_to_verify]) {
    // The permission actually came from the verification descriptor, so use
    // that descriptor when returning the permission status.
    descriptors[internal_index_to_verify] = std::move(verification_descriptor);
  }

  VerifyPermissionsAndReturnStatus(resolver, std::move(descriptors),
                                   std::move(caller_index_to_internal_index),
                                   internal_index_to_verify, is_bulk_request,
                                   std::move(results));
}

PermissionStatusListener* Permissions::GetOrCreatePermissionStatusListener(
    mojom::blink::PermissionStatus status,
    mojom::blink::PermissionDescriptorPtr descriptor) {
  auto type = GetPermissionType(*descriptor);
  if (!type)
    return nullptr;

  if (!listeners_.Contains(*type)) {
    listeners_.insert(
        *type, PermissionStatusListener::Create(*this, GetExecutionContext(),
                                                status, std::move(descriptor)));
  } else {
    listeners_.at(*type)->SetStatus(status);
  }

  return listeners_.at(*type);
}

std::optional<PermissionType> Permissions::GetPermissionType(
    const mojom::blink::PermissionDescriptor& descriptor) {
  return PermissionDescriptorInfoToPermissionType(
      descriptor.name,
      descriptor.extension && descriptor.extension->is_midi() &&
          descriptor.extension->get_midi()->sysex,
      descriptor.extension && descriptor.extension->is_camera_device() &&
          descriptor.extension->get_camera_device()->panTiltZoom,
      descriptor.extension && descriptor.extension->is_clipboard() &&
          descriptor.extension->get_clipboard()->will_be_sanitized,
      descriptor.extension && descriptor.extension->is_clipboard() &&
          descriptor.extension->get_clipboard()->has_user_gesture,
      descriptor.extension && descriptor.extension->is_fullscreen() &&
          descriptor.extension->get_fullscreen()->allow_without_user_gesture);
}

mojom::blink::PermissionDescriptorPtr
Permissions::CreatePermissionVerificationDescriptor(
    PermissionType descriptor_type) {
  if (descriptor_type == PermissionType::CAMERA_PAN_TILT_ZOOM) {
    return CreateVideoCapturePermissionDescriptor(false /* pan_tilt_zoom */);
  }
  return nullptr;
}

}  // namespace blink
