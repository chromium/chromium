// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/managed_device/navigator_managed_data.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

namespace {

const char kNotHighTrustedAppExceptionMessage[] =
    "This API is available only for managed apps.";

}  // namespace

const char NavigatorManagedData::kSupplementName[] = "NavigatorManagedData";

NavigatorManagedData* NavigatorManagedData::managed(Navigator& navigator) {
  if (!navigator.DomWindow())
    return nullptr;

  NavigatorManagedData* device_service =
      Supplement<Navigator>::From<NavigatorManagedData>(navigator);
  if (!device_service) {
    device_service = MakeGarbageCollected<NavigatorManagedData>(navigator);
    ProvideTo(navigator, device_service);
  }
  return device_service;
}

NavigatorManagedData::NavigatorManagedData(Navigator& navigator)
    : ActiveScriptWrappable<NavigatorManagedData>({}),
      Supplement<Navigator>(navigator),
      device_api_service_(navigator.DomWindow()),
      managed_configuration_service_(navigator.DomWindow()),
      configuration_observer_(this, navigator.DomWindow()) {}

const AtomicString& NavigatorManagedData::InterfaceName() const {
  return event_target_names::kNavigatorManagedData;
}

ExecutionContext* NavigatorManagedData::GetExecutionContext() const {
  return GetSupplementable()->DomWindow();
}

bool NavigatorManagedData::HasPendingActivity() const {
  // Prevents garbage collecting of this object when not hold by another
  // object but still has listeners registered.
  return !pending_promises_.empty() || HasEventListeners();
}

void NavigatorManagedData::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ActiveScriptWrappable::Trace(visitor);
  Supplement<Navigator>::Trace(visitor);

  visitor->Trace(device_api_service_);
  visitor->Trace(managed_configuration_service_);
  visitor->Trace(pending_promises_);
  visitor->Trace(configuration_observer_);
}

mojom::blink::DeviceAPIService* NavigatorManagedData::GetService() {
  if (!device_api_service_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        device_api_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // The access status of Device API can change dynamically. Hence, we have to
    // properly handle cases when we are losing this access.
    device_api_service_.set_disconnect_handler(
        WTF::BindOnce(&NavigatorManagedData::OnServiceConnectionError,
                      WrapWeakPersistent(this)));
  }

  return device_api_service_.get();
}

mojom::blink::ManagedConfigurationService*
NavigatorManagedData::GetManagedConfigurationService() {
  if (!managed_configuration_service_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        managed_configuration_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // The access status of Device API can change dynamically. Hence, we have to
    // properly handle cases when we are losing this access.
    managed_configuration_service_.set_disconnect_handler(
        WTF::BindOnce(&NavigatorManagedData::OnServiceConnectionError,
                      WrapWeakPersistent(this)));
  }

  return managed_configuration_service_.get();
}

void NavigatorManagedData::OnServiceConnectionError() {
  device_api_service_.reset();

  // We should reset managed configuration service only it actually got
  // disconnected.
  if (managed_configuration_service_.is_bound() &&
      !managed_configuration_service_.is_connected()) {
    managed_configuration_service_.reset();
  }

  // Resolve all pending promises with a failure.
  for (ScriptPromiseResolver* resolver : pending_promises_) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                           kNotHighTrustedAppExceptionMessage));
  }
}

ScriptPromise NavigatorManagedData::getManagedConfiguration(
    ScriptState* script_state,
    Vector<String> keys) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_promises_.insert(resolver);

  ScriptPromise promise = resolver->Promise();
  if (!GetExecutionContext()) {
    return promise;
  }
  GetManagedConfigurationService()->GetManagedConfiguration(
      keys, WTF::BindOnce(&NavigatorManagedData::OnConfigurationReceived,
                          WrapWeakPersistent(this), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise NavigatorManagedData::getDirectoryId(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_promises_.insert(resolver);

  ScriptPromise promise = resolver->Promise();
  if (!GetExecutionContext()) {
    return promise;
  }
  GetService()->GetDirectoryId(WTF::BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise NavigatorManagedData::getHostname(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_promises_.insert(resolver);

  ScriptPromise promise = resolver->Promise();
  if (!GetExecutionContext()) {
    return promise;
  }
  GetService()->GetHostname(WTF::BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise NavigatorManagedData::getSerialNumber(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_promises_.insert(resolver);

  ScriptPromise promise = resolver->Promise();
  if (!GetExecutionContext()) {
    return promise;
  }
  GetService()->GetSerialNumber(WTF::BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise NavigatorManagedData::getAnnotatedAssetId(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_promises_.insert(resolver);

  ScriptPromise promise = resolver->Promise();
  if (!GetExecutionContext()) {
    return promise;
  }
  GetService()->GetAnnotatedAssetId(WTF::BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise NavigatorManagedData::getAnnotatedLocation(
    ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_promises_.insert(resolver);

  ScriptPromise promise = resolver->Promise();
  if (!GetExecutionContext()) {
    return promise;
  }
  GetService()->GetAnnotatedLocation(WTF::BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

void NavigatorManagedData::OnConfigurationReceived(
    ScriptPromiseResolver* scoped_resolver,
    const absl::optional<HashMap<String, String>>& configurations) {
  pending_promises_.erase(scoped_resolver);

  ScriptState* script_state = scoped_resolver->GetScriptState();
  ScriptState::Scope scope(script_state);

  if (!configurations.has_value()) {
    scoped_resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                           kNotHighTrustedAppExceptionMessage));
    return;
  }

  V8ObjectBuilder result(script_state);
  for (const auto& config_pair : *configurations) {
    v8::Local<v8::Value> v8_object;
    if (v8::JSON::Parse(script_state->GetContext(),
                        V8String(script_state->GetIsolate(), config_pair.value))
            .ToLocal(&v8_object)) {
      result.Add(config_pair.key, v8_object);
    }
  }
  scoped_resolver->Resolve(result.GetScriptValue());
}

void NavigatorManagedData::OnAttributeReceived(
    ScriptState* script_state,
    ScriptPromiseResolver* scoped_resolver,
    mojom::blink::DeviceAttributeResultPtr result) {
  pending_promises_.erase(scoped_resolver);

  if (result->is_error_message()) {
    scoped_resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, result->get_error_message()));
  } else if (result->get_attribute().IsNull()) {
    scoped_resolver->Resolve(v8::Null(script_state->GetIsolate()));
  } else {
    scoped_resolver->Resolve(result->get_attribute());
  }
}

void NavigatorManagedData::OnConfigurationChanged() {
  DispatchEvent(*Event::Create(event_type_names::kManagedconfigurationchange));
}

void NavigatorManagedData::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  if (!GetExecutionContext()) {
    return;
  }

  EventTargetWithInlineData::AddedEventListener(event_type,
                                                registered_listener);
  if (event_type == event_type_names::kManagedconfigurationchange) {
    if (!configuration_observer_.is_bound()) {
      GetManagedConfigurationService()->SubscribeToManagedConfiguration(
          configuration_observer_.BindNewPipeAndPassRemote(
              GetExecutionContext()->GetTaskRunner(
                  TaskType::kMiscPlatformAPI)));
    }
  }
}

void NavigatorManagedData::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTargetWithInlineData::RemovedEventListener(event_type,
                                                  registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

void NavigatorManagedData::StopObserving() {
  if (!configuration_observer_.is_bound())
    return;
  configuration_observer_.reset();
}

}  // namespace blink
