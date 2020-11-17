// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permissions.h"

#include <memory>
#include <utility>

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
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/modules/permissions/permission_status.h"
#include "third_party/blink/renderer/modules/permissions/permission_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
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
      service_(navigator.GetExecutionContext()) {}

ScriptPromise Permissions::query(ScriptState* script_state,
                                 const ScriptValue& raw_permission,
                                 ExceptionState& exception_state) {
  PermissionDescriptorPtr descriptor =
      ParsePermissionDescriptor(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // If the current origin is a file scheme, it will unlikely return a
  // meaningful value because most APIs are broken on file scheme and no
  // permission prompt will be shown even if the returned permission will most
  // likely be "prompt".
  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  GetService(ExecutionContext::From(script_state))
      ->HasPermission(std::move(descriptor),
                      WTF::Bind(&Permissions::TaskComplete,
                                WrapPersistent(this), WrapPersistent(resolver),
                                WTF::Passed(std::move(descriptor_copy))));
  return promise;
}

ScriptPromise Permissions::request(ScriptState* script_state,
                                   const ScriptValue& raw_permission,
                                   ExceptionState& exception_state) {
  PermissionDescriptorPtr descriptor =
      ParsePermissionDescriptor(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  ExecutionContext* context = ExecutionContext::From(script_state);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);
  LocalFrame* frame = window ? window->GetFrame() : nullptr;
  GetService(context)->RequestPermission(
      std::move(descriptor), LocalFrame::HasTransientUserActivation(frame),
      WTF::Bind(&Permissions::TaskComplete, WrapPersistent(this),
                WrapPersistent(resolver),
                WTF::Passed(std::move(descriptor_copy))));
  return promise;
}

ScriptPromise Permissions::revoke(ScriptState* script_state,
                                  const ScriptValue& raw_permission,
                                  ExceptionState& exception_state) {
  PermissionDescriptorPtr descriptor =
      ParsePermissionDescriptor(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  GetService(ExecutionContext::From(script_state))
      ->RevokePermission(
          std::move(descriptor),
          WTF::Bind(&Permissions::TaskComplete, WrapPersistent(this),
                    WrapPersistent(resolver),
                    WTF::Passed(std::move(descriptor_copy))));
  return promise;
}

ScriptPromise Permissions::requestAll(
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
      return ScriptPromise();

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

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  Vector<PermissionDescriptorPtr> internal_permissions_copy;
  internal_permissions_copy.ReserveCapacity(internal_permissions.size());
  for (const auto& descriptor : internal_permissions)
    internal_permissions_copy.push_back(descriptor->Clone());

  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);
  LocalFrame* frame = window ? window->GetFrame() : nullptr;
  GetService(context)->RequestPermissions(
      std::move(internal_permissions),
      LocalFrame::HasTransientUserActivation(frame),
      WTF::Bind(&Permissions::BatchTaskComplete, WrapPersistent(this),
                WrapPersistent(resolver),
                WTF::Passed(std::move(internal_permissions_copy)),
                WTF::Passed(std::move(caller_index_to_internal_index))));
  return promise;
}

void Permissions::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
}

PermissionService* Permissions::GetService(
    ExecutionContext* execution_context) {
  if (!service_.is_bound()) {
    ConnectToPermissionService(
        execution_context,
        service_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kPermission)));
    service_.set_disconnect_handler(WTF::Bind(
        &Permissions::ServiceConnectionError, WrapWeakPersistent(this)));
  }
  return service_.get();
}

void Permissions::ServiceConnectionError() {
  service_.reset();
}

void Permissions::TaskComplete(ScriptPromiseResolver* resolver,
                               mojom::blink::PermissionDescriptorPtr descriptor,
                               mojom::blink::PermissionStatus result) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  resolver->Resolve(
      PermissionStatus::Take(resolver, result, std::move(descriptor)));
}

void Permissions::BatchTaskComplete(
    ScriptPromiseResolver* resolver,
    Vector<mojom::blink::PermissionDescriptorPtr> descriptors,
    Vector<int> caller_index_to_internal_index,
    const Vector<mojom::blink::PermissionStatus>& results) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;

  // Create the response vector by finding the status for each index by
  // using the caller to internal index mapping and looking up the status
  // using the internal index obtained.
  HeapVector<Member<PermissionStatus>> result;
  result.ReserveInitialCapacity(caller_index_to_internal_index.size());
  for (int internal_index : caller_index_to_internal_index) {
    result.push_back(PermissionStatus::CreateAndListen(
        resolver->GetExecutionContext(), results[internal_index],
        descriptors[internal_index]->Clone()));
  }
  resolver->Resolve(result);
}

}  // namespace blink
