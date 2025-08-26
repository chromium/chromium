// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/managed_device/navigator_managed_data.h"

#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

constexpr char kNotHighTrustedAppExceptionMessage[] =
    "Managed configuration is empty. This API is available only for "
    "managed apps.";
constexpr char kServiceConnectionExceptionMessage[] =
    "Service connection error. This API is available only for managed apps.";

#if BUILDFLAG(IS_ANDROID)
constexpr char kManagedConfigNotSupported[] =
    "Managed Configuration API is not supported on this platform.";
#endif  // BUILDFLAG(IS_ANDROID)

constexpr char kDeviceAttributesNotAllowedByPermissionsPolicy[] =
    "Permissions-Policy: device-attributes are disabled.";

constexpr char kDeviceAttributesNotAllowedInChildFrames[] =
    "This API is allowed only in top level frames.";

bool IsDeviceAttributesPermissionsPolicyFeatureEnabled() {
  return RuntimeEnabledFeatures::DeviceAttributesPermissionPolicyEnabled();
}

bool AreDeviceAttributesAllowedByPermissionsPolicy(ExecutionContext* context) {
  if (!IsDeviceAttributesPermissionsPolicyFeatureEnabled()) {
    return true;
  }
  return context->IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kDeviceAttributes);
}

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
  EventTarget::Trace(visitor);
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
        BindOnce(&NavigatorManagedData::OnServiceConnectionError,
                 WrapWeakPersistent(this)));
  }

  return device_api_service_.get();
}

#if !BUILDFLAG(IS_ANDROID)
mojom::blink::ManagedConfigurationService*
NavigatorManagedData::GetManagedConfigurationService() {
  if (!managed_configuration_service_.is_bound()) {
    GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
        managed_configuration_service_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // The access status of Device API can change dynamically. Hence, we have to
    // properly handle cases when we are losing this access.
    managed_configuration_service_.set_disconnect_handler(
        BindOnce(&NavigatorManagedData::OnServiceConnectionError,
                 WrapWeakPersistent(this)));
  }

  return managed_configuration_service_.get();
}
#endif  // !BUILDFLAG(IS_ANDROID)

void NavigatorManagedData::OnServiceConnectionError() {
  device_api_service_.reset();

  // We should reset managed configuration service only it actually got
  // disconnected.
  if (managed_configuration_service_.is_bound() &&
      !managed_configuration_service_.is_connected()) {
    managed_configuration_service_.reset();
  }

  // Resolve all pending promises with a failure.
  for (ScriptPromiseResolverBase* resolver : pending_promises_) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                           kServiceConnectionExceptionMessage));
  }
}

ScriptPromise<IDLRecord<IDLString, IDLAny>>
NavigatorManagedData::getManagedConfiguration(ScriptState* script_state,
                                              Vector<String> keys) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLRecord<IDLString, IDLAny>>>(
          script_state);
  pending_promises_.insert(resolver);

  auto promise = resolver->Promise();
  if (!GetExecutionContext()) {
    return promise;
  }
#if !BUILDFLAG(IS_ANDROID)
  GetManagedConfigurationService()->GetManagedConfiguration(
      keys, BindOnce(&NavigatorManagedData::OnConfigurationReceived,
                     WrapWeakPersistent(this), WrapPersistent(resolver)));
#else
  resolver->Reject(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotSupportedError, kManagedConfigNotSupported));
#endif  // !BUILDFLAG(IS_ANDROID)

  return promise;
}

ScriptPromise<IDLNullable<IDLString>> NavigatorManagedData::getDirectoryId(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!CheckDeviceAttributesAllowed(exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<IDLString>>>(
          script_state);
  pending_promises_.insert(resolver);
  auto promise = resolver->Promise();

  GetService()->GetDirectoryId(BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLNullable<IDLString>> NavigatorManagedData::getHostname(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!CheckDeviceAttributesAllowed(exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<IDLString>>>(
          script_state);
  pending_promises_.insert(resolver);
  auto promise = resolver->Promise();

  GetService()->GetHostname(BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLNullable<IDLString>> NavigatorManagedData::getSerialNumber(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!CheckDeviceAttributesAllowed(exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<IDLString>>>(
          script_state);
  pending_promises_.insert(resolver);
  auto promise = resolver->Promise();

  GetService()->GetSerialNumber(BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLNullable<IDLString>> NavigatorManagedData::getAnnotatedAssetId(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  if (!CheckDeviceAttributesAllowed(exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<IDLString>>>(
          script_state);
  pending_promises_.insert(resolver);
  auto promise = resolver->Promise();

  GetService()->GetAnnotatedAssetId(BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

ScriptPromise<IDLNullable<IDLString>>
NavigatorManagedData::getAnnotatedLocation(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  if (!CheckDeviceAttributesAllowed(exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLNullable<IDLString>>>(
          script_state);
  pending_promises_.insert(resolver);
  auto promise = resolver->Promise();

  GetService()->GetAnnotatedLocation(BindOnce(
      &NavigatorManagedData::OnAttributeReceived, WrapWeakPersistent(this),
      WrapPersistent(script_state), WrapPersistent(resolver)));
  return promise;
}

bool NavigatorManagedData::CheckDeviceAttributesAllowed(
    ExceptionState& exception_state) {
  ExecutionContext* const context = GetExecutionContext();
  if (!context) {
    return false;
  }
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(context);
  CHECK(window);
  if (!window->GetFrame()->IsMainFrame() ||
      window->GetFrame()->IsInFencedFrameTree()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kDeviceAttributesNotAllowedInChildFrames);
    return false;
  }
  if (!AreDeviceAttributesAllowedByPermissionsPolicy(context)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        kDeviceAttributesNotAllowedByPermissionsPolicy);
    return false;
  }
  return true;
}

void NavigatorManagedData::OnConfigurationReceived(
    ScriptPromiseResolver<IDLRecord<IDLString, IDLAny>>* resolver,
    const std::optional<HashMap<String, String>>& configurations) {
  pending_promises_.erase(resolver);

  ScriptState* script_state = resolver->GetScriptState();
  ScriptState::Scope scope(script_state);

  if (!configurations.has_value()) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                           kNotHighTrustedAppExceptionMessage));
    return;
  }

  HeapVector<std::pair<String, ScriptValue>> result;
  for (const auto& config_pair : *configurations) {
    v8::Local<v8::Value> v8_object =
        FromJSONString(script_state, config_pair.value);
    if (!v8_object.IsEmpty()) {
      result.emplace_back(config_pair.key,
                          ScriptValue(script_state->GetIsolate(), v8_object));
    }
  }
  resolver->Resolve(result);
}

void NavigatorManagedData::OnAttributeReceived(
    ScriptState* script_state,
    ScriptPromiseResolver<IDLNullable<IDLString>>* resolver,
    mojom::blink::DeviceAttributeResultPtr result) {
  pending_promises_.erase(resolver);

  if (result->is_error_message()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kUnknownError, result->get_error_message()));
  } else {
    resolver->Resolve(result->get_attribute());
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

  EventTarget::AddedEventListener(event_type, registered_listener);
#if !BUILDFLAG(IS_ANDROID)
  if (event_type == event_type_names::kManagedconfigurationchange) {
    if (!configuration_observer_.is_bound()) {
      GetManagedConfigurationService()->SubscribeToManagedConfiguration(
          configuration_observer_.BindNewPipeAndPassRemote(
              GetExecutionContext()->GetTaskRunner(
                  TaskType::kMiscPlatformAPI)));
    }
  }
#else
  GetExecutionContext()->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kOther,
      mojom::blink::ConsoleMessageLevel::kWarning, kManagedConfigNotSupported));
#endif  // !BUILDFLAG(IS_ANDROID)
}

void NavigatorManagedData::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  if (!HasEventListeners())
    StopObserving();
}

void NavigatorManagedData::StopObserving() {
  if (!configuration_observer_.is_bound())
    return;
  configuration_observer_.reset();
}

}  // namespace blink
