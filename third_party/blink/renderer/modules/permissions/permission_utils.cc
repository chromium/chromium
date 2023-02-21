// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permission_utils.h"

#include <utility>

#include "build/build_config.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_camera_device_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_midi_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_push_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_top_level_storage_access_permission_descriptor.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// There are two PermissionDescriptor, one in Mojo bindings and one
// in v8 bindings so we'll rename one here.
using MojoPermissionDescriptor = mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionStatus;

void ConnectToPermissionService(
    ExecutionContext* execution_context,
    mojo::PendingReceiver<mojom::blink::PermissionService> receiver) {
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      std::move(receiver));
}

String PermissionStatusToString(PermissionStatus status) {
  switch (status) {
    case PermissionStatus::GRANTED:
      return "granted";
    case PermissionStatus::DENIED:
      return "denied";
    case PermissionStatus::ASK:
      return "prompt";
  }
  NOTREACHED();
  return "denied";
}

String PermissionNameToString(PermissionName name) {
  // TODO(crbug.com/1395451): Change these strings to match the JS permission
  // strings (dashes instead of underscores).
  switch (name) {
    case PermissionName::GEOLOCATION:
      return "geolocation";
    case PermissionName::NOTIFICATIONS:
      return "notifications";
    case PermissionName::MIDI:
      return "midi";
    case PermissionName::PROTECTED_MEDIA_IDENTIFIER:
      return "protected_media_identifier";
    case PermissionName::DURABLE_STORAGE:
      return "durable_storage";
    case PermissionName::AUDIO_CAPTURE:
      return "audio_capture";
    case PermissionName::VIDEO_CAPTURE:
      return "video_capture";
    case PermissionName::BACKGROUND_SYNC:
      return "background_sync";
    case PermissionName::SENSORS:
      return "sensors";
    case PermissionName::ACCESSIBILITY_EVENTS:
      return "accessibility_events";
    case PermissionName::CLIPBOARD_READ:
      return "clipboard_read";
    case PermissionName::CLIPBOARD_WRITE:
      return "clipboard_write";
    case PermissionName::PAYMENT_HANDLER:
      return "payment_handler";
    case PermissionName::BACKGROUND_FETCH:
      return "background_fetch";
    case PermissionName::IDLE_DETECTION:
      return "idle_detection";
    case PermissionName::PERIODIC_BACKGROUND_SYNC:
      return "periodic_background_sync";
    case PermissionName::SCREEN_WAKE_LOCK:
      return "screen_wake_lock";
    case PermissionName::SYSTEM_WAKE_LOCK:
      return "system_wake_lock";
    case PermissionName::NFC:
      return "nfc";
    case PermissionName::STORAGE_ACCESS:
      return "storage-access";
    case PermissionName::WINDOW_MANAGEMENT:
      if (RuntimeEnabledFeatures::WindowManagementPermissionAliasEnabled()) {
        return "window-management";
      }
      return "window_placement";
    case PermissionName::LOCAL_FONTS:
      return "local_fonts";
    case PermissionName::DISPLAY_CAPTURE:
      return "display_capture";
    case PermissionName::TOP_LEVEL_STORAGE_ACCESS:
      return "top-level-storage-access";
  }
  NOTREACHED();
  return "unknown";
}

PermissionDescriptorPtr CreatePermissionDescriptor(PermissionName name) {
  auto descriptor = MojoPermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

PermissionDescriptorPtr CreateMidiPermissionDescriptor(bool sysex) {
  auto descriptor = CreatePermissionDescriptor(PermissionName::MIDI);
  auto midi_extension = mojom::blink::MidiPermissionDescriptor::New();
  midi_extension->sysex = sysex;
  descriptor->extension = mojom::blink::PermissionDescriptorExtension::NewMidi(
      std::move(midi_extension));
  return descriptor;
}

PermissionDescriptorPtr CreateClipboardPermissionDescriptor(
    PermissionName name,
    bool has_user_gesture,
    bool will_be_sanitized) {
  auto descriptor = CreatePermissionDescriptor(name);
  auto clipboard_extension = mojom::blink::ClipboardPermissionDescriptor::New(
      has_user_gesture, will_be_sanitized);
  descriptor->extension =
      mojom::blink::PermissionDescriptorExtension::NewClipboard(
          std::move(clipboard_extension));
  return descriptor;
}

PermissionDescriptorPtr CreateVideoCapturePermissionDescriptor(
    bool pan_tilt_zoom) {
  auto descriptor = CreatePermissionDescriptor(PermissionName::VIDEO_CAPTURE);
  auto camera_device_extension =
      mojom::blink::CameraDevicePermissionDescriptor::New(pan_tilt_zoom);
  descriptor->extension =
      mojom::blink::PermissionDescriptorExtension::NewCameraDevice(
          std::move(camera_device_extension));
  return descriptor;
}

PermissionDescriptorPtr CreateTopLevelStorageAccessPermissionDescriptor(
    const KURL& origin_as_kurl) {
  auto descriptor =
      CreatePermissionDescriptor(PermissionName::TOP_LEVEL_STORAGE_ACCESS);
  scoped_refptr<SecurityOrigin> supplied_origin =
      SecurityOrigin::Create(origin_as_kurl);
  auto top_level_storage_access_extension =
      mojom::blink::TopLevelStorageAccessPermissionDescriptor::New();
  top_level_storage_access_extension->requestedOrigin = supplied_origin;
  descriptor->extension =
      mojom::blink::PermissionDescriptorExtension::NewTopLevelStorageAccess(
          std::move(top_level_storage_access_extension));
  return descriptor;
}

PermissionDescriptorPtr ParsePermissionDescriptor(
    ScriptState* script_state,
    const ScriptValue& raw_descriptor,
    ExceptionState& exception_state) {
  PermissionDescriptor* permission =
      NativeValueTraits<PermissionDescriptor>::NativeValue(
          script_state->GetIsolate(), raw_descriptor.V8Value(),
          exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  const String& name = permission->name();
  if (name == "geolocation") {
    return CreatePermissionDescriptor(PermissionName::GEOLOCATION);
  }
  if (name == "camera") {
    CameraDevicePermissionDescriptor* camera_device_permission =
        NativeValueTraits<CameraDevicePermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_descriptor.V8Value(),
            exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }

    return CreateVideoCapturePermissionDescriptor(
        camera_device_permission->panTiltZoom());
  }
  if (name == "microphone") {
    return CreatePermissionDescriptor(PermissionName::AUDIO_CAPTURE);
  }
  if (name == "notifications") {
    return CreatePermissionDescriptor(PermissionName::NOTIFICATIONS);
  }
  if (name == "persistent-storage") {
    return CreatePermissionDescriptor(PermissionName::DURABLE_STORAGE);
  }
  if (name == "push") {
    PushPermissionDescriptor* push_permission =
        NativeValueTraits<PushPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_descriptor.V8Value(),
            exception_state);
    if (exception_state.HadException()) {
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
            script_state->GetIsolate(), raw_descriptor.V8Value(),
            exception_state);
    return CreateMidiPermissionDescriptor(midi_permission->sysex());
  }
  if (name == "background-sync") {
    return CreatePermissionDescriptor(PermissionName::BACKGROUND_SYNC);
  }
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
    if (name == "clipboard-write") {
      permission_name = PermissionName::CLIPBOARD_WRITE;
    }

    ClipboardPermissionDescriptor* clipboard_permission =
        NativeValueTraits<ClipboardPermissionDescriptor>::NativeValue(
            script_state->GetIsolate(), raw_descriptor.V8Value(),
            exception_state);
    return CreateClipboardPermissionDescriptor(
        permission_name,
        /*has_user_gesture=*/!clipboard_permission->allowWithoutGesture(),
        /*will_be_sanitized=*/
        !clipboard_permission->allowWithoutSanitization());
  }
  if (name == "payment-handler") {
    return CreatePermissionDescriptor(PermissionName::PAYMENT_HANDLER);
  }
  if (name == "background-fetch") {
    return CreatePermissionDescriptor(PermissionName::BACKGROUND_FETCH);
  }
  if (name == "idle-detection") {
    return CreatePermissionDescriptor(PermissionName::IDLE_DETECTION);
  }
  if (name == "periodic-background-sync") {
    return CreatePermissionDescriptor(PermissionName::PERIODIC_BACKGROUND_SYNC);
  }
  if (name == "screen-wake-lock") {
    return CreatePermissionDescriptor(PermissionName::SCREEN_WAKE_LOCK);
  }
  if (name == "system-wake-lock") {
    if (!RuntimeEnabledFeatures::SystemWakeLockEnabled(
            ExecutionContext::From(script_state))) {
      exception_state.ThrowTypeError("System Wake Lock is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::SYSTEM_WAKE_LOCK);
  }
  if (name == "nfc") {
    if (!RuntimeEnabledFeatures::WebNFCEnabled(
            ExecutionContext::From(script_state))) {
      exception_state.ThrowTypeError("Web NFC is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::NFC);
  }
  if (name == "storage-access") {
    if (!RuntimeEnabledFeatures::StorageAccessAPIEnabled()) {
      exception_state.ThrowTypeError("The Storage Access API is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::STORAGE_ACCESS);
  }
  if (name == "top-level-storage-access") {
    if (!RuntimeEnabledFeatures::StorageAccessAPIEnabled() ||
        !RuntimeEnabledFeatures::StorageAccessAPIForOriginExtensionEnabled()) {
      exception_state.ThrowTypeError(
          "The requestStorageAccessForOrigin API is not enabled.");
      return nullptr;
    }
    TopLevelStorageAccessPermissionDescriptor*
        top_level_storage_access_permission =
            NativeValueTraits<TopLevelStorageAccessPermissionDescriptor>::
                NativeValue(script_state->GetIsolate(),
                            raw_descriptor.V8Value(), exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }
    KURL origin_as_kurl{top_level_storage_access_permission->requestedOrigin()};
    if (!origin_as_kurl.IsValid()) {
      exception_state.ThrowTypeError("The requested origin is invalid.");
      return nullptr;
    }

    return CreateTopLevelStorageAccessPermissionDescriptor(origin_as_kurl);
  }
  if (name == "window-management") {
    UseCounter::Count(CurrentExecutionContext(script_state->GetIsolate()),
                      WebFeature::kWindowManagementPermissionDescriptorUsed);
    if (!RuntimeEnabledFeatures::WindowManagementPermissionAliasEnabled()) {
      exception_state.ThrowTypeError(
          "The Window Management alias is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::WINDOW_MANAGEMENT);
  }
  if (name == "window-placement") {
    UseCounter::Count(CurrentExecutionContext(script_state->GetIsolate()),
                      WebFeature::kWindowPlacementPermissionDescriptorUsed);
    return CreatePermissionDescriptor(PermissionName::WINDOW_MANAGEMENT);
  }
  if (name == "local-fonts") {
    if (!RuntimeEnabledFeatures::FontAccessEnabled(
            ExecutionContext::From(script_state))) {
      exception_state.ThrowTypeError("Local Fonts Access API is not enabled.");
      return nullptr;
    }
    return CreatePermissionDescriptor(PermissionName::LOCAL_FONTS);
  }
  if (name == "display-capture") {
    return CreatePermissionDescriptor(PermissionName::DISPLAY_CAPTURE);
  }
  return nullptr;
}

}  // namespace blink
