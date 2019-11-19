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
#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_midi_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_push_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_wake_lock_permission_descriptor.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/modules/permissions/permission_descriptor.h"
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

namespace {

// Parses the raw permission dictionary and returns the Mojo
// PermissionDescriptor if parsing was successful. If an exception occurs, it
// will be stored in |exceptionState| and null will be returned. Therefore, the
// |exceptionState| should be checked before attempting to use the returned
// permission as the non-null assert will be fired otherwise.
//
// Websites will be able to run code when `name()` is called, changing the
// current context. The caller should make sure that no assumption is made
// after this has been called.
PermissionDescriptorPtr ParsePermission(ScriptState* script_state,
                                        const ScriptValue raw_permission,
                                        ExceptionState& exception_state) {
  PermissionDescriptor* permission =
      NativeValueTraits<PermissionDescriptor>::NativeValue(
          script_state->GetIsolate(), raw_permission.V8Value(),
          exception_state);

  if (exception_state.HadException()) {
    exception_state.ThrowTypeError(exception_state.Message());
    return nullptr;
  }

  const String& name = permission->name();
  if (name == "geolocation")
    return CreatePermissionDescriptor(PermissionName::GEOLOCATION);
  if (name == "camera")
    return CreatePermissionDescriptor(PermissionName::VIDEO_CAPTURE);
  if (name == "microphone")
    return CreatePermissionDescriptor(PermissionName::AUDIO_CAPTURE);
  if (name == "notifications")
    return CreatePermissionDescriptor(PermissionName::NOTIFICATIONS);
  if (name == "persistent-storage")
    return CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE);
  if (name == "push") {
    PushPermissionDescriptor* push_permission =
        NativeValueTraits<PushPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_permission.V8Value(),
            exception_state);
    if (exception_state.HadException()) {
      exception_state.ThrowTypeError(exception_state.Message());
      return nullptr;
    }

    // Only "userVisibleOnly" push is supported for now.
    if (!push_permission->userVisibleOnly()) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "Push Permission without userVisibleOnly:true isn't supported yet.");
      return nullptr;
    }

    return CreatePermissionDescriptor(PermissionName::NOTIFICATIONS);
  }
  if (name == "midi") {
    MidiPermissionDescriptor* midi_permission =
        NativeValueTraits<MidiPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_permission.V8Value(),
            exception_state);
    return CreateMidiPermissionDescriptor(midi_permission->sysex());
  }
  if (name == "background-sync")
    return CreatePermissionDescriptor(PermissionName::BACKGROUND_SYNC);
  if (name == "ambient-light-sensor" || name == "accelerometer" ||
      name == "gyroscope" || name == "magnetometer") {
    // ALS requires an extra flag.
    if (name == "ambient-light-sensor") {
      if (!RuntimeEnabledFeatures::SensorExtraClassesEnabled()) {
        exception_state.ThrowTypeError(
            "GenericSensorExtraClasses flag is not enabled.");
        return nullptr;
      }
    }

    return CreatePermissionDescriptor(PermissionName::SENSORS);
  }
  if (name == "accessibility-events") {
    if (!RuntimeEnabledFeatures::AccessibilityObjectModelEnabled()) {
      exception_state.ThrowTypeError(
          "Accessibility Object Model is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::ACCESSIBILITY_EVENTS);
  }
  if (name == "clipboard-read" || name == "clipboard-write") {
    PermissionName permission_name = PermissionName::CLIPBOARD_READ;
    if (name == "clipboard-write")
      permission_name = PermissionName::CLIPBOARD_WRITE;

    ClipboardPermissionDescriptor* clipboard_permission =
        NativeValueTraits<ClipboardPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_permission.V8Value(),
            exception_state);
    return CreateClipboardPermissionDescriptor(
        permission_name, clipboard_permission->allowWithoutGesture());
  }
  if (name == "payment-handler")
    return CreatePermissionDescriptor(PermissionName::PAYMENT_HANDLER);
  if (name == "background-fetch")
    return CreatePermissionDescriptor(PermissionName::BACKGROUND_FETCH);
  if (name == "idle-detection")
    return CreatePermissionDescriptor(PermissionName::IDLE_DETECTION);
  if (name == "periodic-background-sync") {
    if (!RuntimeEnabledFeatures::PeriodicBackgroundSyncEnabled(
            ExecutionContext::From(script_state))) {
      exception_state.ThrowTypeError(
          "Periodic Background Sync is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::PERIODIC_BACKGROUND_SYNC);
  }
  if (name == "wake-lock") {
    if (!RuntimeEnabledFeatures::WakeLockEnabled(
            ExecutionContext::From(script_state))) {
      exception_state.ThrowTypeError("Wake Lock is not enabled.");
      return nullptr;
    }
    WakeLockPermissionDescriptor* wake_lock_permission =
        NativeValueTraits<WakeLockPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_permission.V8Value(),
            exception_state);
    if (exception_state.HadException())
      return nullptr;
    const String& type = wake_lock_permission->type();
    if (type == "screen") {
      return CreateWakeLockPermissionDescriptor(
          mojom::blink::WakeLockType::kScreen);
    } else if (type == "system") {
      return CreateWakeLockPermissionDescriptor(
          mojom::blink::WakeLockType::kSystem);
    } else {
      NOTREACHED();
    }
  }
  if (name == "nfc") {
    if (!RuntimeEnabledFeatures::WebNFCEnabled(
            ExecutionContext::From(script_state))) {
      exception_state.ThrowTypeError("Web NFC is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::NFC);
  }
  return nullptr;
}

}  // anonymous namespace

ScriptPromise Permissions::query(ScriptState* script_state,
                                 const ScriptValue& raw_permission,
                                 ExceptionState& exception_state) {
  PermissionDescriptorPtr descriptor =
      ParsePermission(script_state, raw_permission, exception_state);
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
      ParsePermission(script_state, raw_permission, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  ExecutionContext* context = ExecutionContext::From(script_state);

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  PermissionDescriptorPtr descriptor_copy = descriptor->Clone();
  Document* doc = DynamicTo<Document>(context);
  LocalFrame* frame = doc ? doc->GetFrame() : nullptr;
  GetService(ExecutionContext::From(script_state))
      ->RequestPermission(
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
      ParsePermission(script_state, raw_permission, exception_state);
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

    auto descriptor =
        ParsePermission(script_state, raw_permission, exception_state);
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

  Document* doc = DynamicTo<Document>(context);
  LocalFrame* frame = doc ? doc->GetFrame() : nullptr;
  GetService(ExecutionContext::From(script_state))
      ->RequestPermissions(
          std::move(internal_permissions),
          LocalFrame::HasTransientUserActivation(frame),
          WTF::Bind(&Permissions::BatchTaskComplete, WrapPersistent(this),
                    WrapPersistent(resolver),
                    WTF::Passed(std::move(internal_permissions_copy)),
                    WTF::Passed(std::move(caller_index_to_internal_index))));
  return promise;
}

PermissionService* Permissions::GetService(
    ExecutionContext* execution_context) {
  if (!service_) {
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
