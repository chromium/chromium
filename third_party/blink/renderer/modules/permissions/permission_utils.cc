// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/permissions/permission_utils.h"

#include <utility>

#include "build/build_config.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_permission_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_camera_device_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_fullscreen_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_midi_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_permission_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_permission_name.h"
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

namespace {

constexpr V8PermissionState::Enum ToPermissionStateEnum(
    mojom::blink::PermissionStatus status) {
  // This assertion protects against the IDL enum changing without updating the
  // corresponding mojom interface, while the lack of a default case in the
  // switch statement below ensures the opposite.
  static_assert(
      V8PermissionState::kEnumSize == 3u,
      "the number of fields in the PermissionStatus mojom enum "
      "must match the number of fields in the PermissionState blink enum");

  switch (status) {
    case mojom::blink::PermissionStatus::GRANTED:
      return V8PermissionState::Enum::kGranted;
    case mojom::blink::PermissionStatus::DENIED:
      return V8PermissionState::Enum::kDenied;
    case mojom::blink::PermissionStatus::ASK:
      return V8PermissionState::Enum::kPrompt;
  }
}

}  // namespace

// There are two PermissionDescriptor, one in Mojo bindings and one
// in v8 bindings so we'll rename one here.
using MojoPermissionDescriptor = mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;

void ConnectToPermissionService(
    ExecutionContext* execution_context,
    mojo::PendingReceiver<mojom::blink::PermissionService> receiver) {
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      std::move(receiver));
}

V8PermissionState ToV8PermissionState(mojom::blink::PermissionStatus status) {
  return V8PermissionState(ToPermissionStateEnum(status));
}

String PermissionNameToString(PermissionName name) {
  // TODO(crbug.com/1395451): Change these strings to match the JS permission
  // strings (dashes instead of underscores).
  switch (name) {
    case PermissionName::GEOLOCATION:
      return "geolocation";
    case PermissionName::GEOLOCATION_APPROXIMATE:
      return "geolocation-approximate";
    case PermissionName::NOTIFICATIONS:
      return "notifications";
    case PermissionName::MIDI:
      return "midi";
    case PermissionName::PROTECTED_MEDIA_IDENTIFIER:
      return "protected_media_identifier";
    case PermissionName::PERSISTENT_STORAGE:
      return "durable_storage";
    case PermissionName::AUDIO_CAPTURE:
      return "audio_capture";
    case PermissionName::VIDEO_CAPTURE:
      return "video_capture";
    case PermissionName::BACKGROUND_SYNC:
      return "background_sync";
    case PermissionName::SENSORS:
      return "sensors";
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
      return "window-management";
    case PermissionName::LOCAL_FONTS:
      return "local_fonts";
    case PermissionName::DISPLAY_CAPTURE:
      return "display_capture";
    case PermissionName::TOP_LEVEL_STORAGE_ACCESS:
      return "top-level-storage-access";
    case PermissionName::CAPTURED_SURFACE_CONTROL:
      return "captured-surface-control";
    case PermissionName::SPEAKER_SELECTION:
      return "speaker-selection";
    case PermissionName::KEYBOARD_LOCK:
      return "keyboard-lock";
    case PermissionName::POINTER_LOCK:
      return "pointer-lock";
    case PermissionName::FULLSCREEN:
      return "fullscreen";
    case PermissionName::WEB_APP_INSTALLATION:
      return "web-app-installation";
    case PermissionName::LOCAL_NETWORK_ACCESS:
      return "local-network-access";
    case PermissionName::LOCAL_NETWORK:
      return "local-network";
    case PermissionName::LOOPBACK_NETWORK:
      return "loopback-network";
    case PermissionName::VR:
      return "vr";
    case PermissionName::AR:
      return "ar";
    case PermissionName::HAND_TRACKING:
      return "hand-tracking";
    case PermissionName::WEB_PRINTING:
      return "web-printing";
    case PermissionName::SMART_CARD:
      return "smart-card";
  }
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

PermissionDescriptorPtr CreateFullscreenPermissionDescriptor(
    bool allow_without_user_gesture) {
  auto descriptor = CreatePermissionDescriptor(PermissionName::FULLSCREEN);
  auto fullscreen_extension = mojom::blink::FullscreenPermissionDescriptor::New(
      allow_without_user_gesture);
  descriptor->extension =
      mojom::blink::PermissionDescriptorExtension::NewFullscreen(
          std::move(fullscreen_extension));
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

  const auto& name = permission->name();

  switch (name.AsEnum()) {
    case V8PermissionName::Enum::kGeolocation:
      return CreatePermissionDescriptor(PermissionName::GEOLOCATION);

    case V8PermissionName::Enum::kNotifications:
      return CreatePermissionDescriptor(PermissionName::NOTIFICATIONS);

    case V8PermissionName::Enum::kPush: {
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
            "Push Permission without userVisibleOnly:true isn't supported "
            "yet.");
        return nullptr;
      }

      return CreatePermissionDescriptor(PermissionName::NOTIFICATIONS);
    }

    case V8PermissionName::Enum::kMidi: {
      MidiPermissionDescriptor* midi_permission =
          NativeValueTraits<MidiPermissionDescriptor>::NativeValue(
              script_state->GetIsolate(), raw_descriptor.V8Value(),
              exception_state);
      return CreateMidiPermissionDescriptor(midi_permission->sysex());
    }

    case V8PermissionName::Enum::kCamera: {
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

    case V8PermissionName::Enum::kMicrophone:
      return CreatePermissionDescriptor(PermissionName::AUDIO_CAPTURE);

    case V8PermissionName::Enum::kBackgroundFetch:
      return CreatePermissionDescriptor(PermissionName::BACKGROUND_FETCH);

    case V8PermissionName::Enum::kBackgroundSync:
      return CreatePermissionDescriptor(PermissionName::BACKGROUND_SYNC);

    case V8PermissionName::Enum::kPersistentStorage:
      return CreatePermissionDescriptor(PermissionName::PERSISTENT_STORAGE);

    case V8PermissionName::Enum::kAmbientLightSensor:
      if (!RuntimeEnabledFeatures::SensorExtraClassesEnabled()) {
        exception_state.ThrowTypeError(
            "GenericSensorExtraClasses flag is not enabled.");
        return nullptr;
      }
      [[fallthrough]];
    case V8PermissionName::Enum::kAccelerometer:
    case V8PermissionName::Enum::kGyroscope:
    case V8PermissionName::Enum::kMagnetometer:
      return CreatePermissionDescriptor(PermissionName::SENSORS);

    case V8PermissionName::Enum::kScreenWakeLock:
      return CreatePermissionDescriptor(PermissionName::SCREEN_WAKE_LOCK);

    case V8PermissionName::Enum::kNfc: {
      if (!RuntimeEnabledFeatures::WebNFCEnabled(
              ExecutionContext::From(script_state))) {
        exception_state.ThrowTypeError("Web NFC is not enabled.");
        return nullptr;
      }
      return CreatePermissionDescriptor(PermissionName::NFC);
    }

    case V8PermissionName::Enum::kDisplayCapture:
      return CreatePermissionDescriptor(PermissionName::DISPLAY_CAPTURE);

    case V8PermissionName::Enum::kClipboardRead: {
      ClipboardPermissionDescriptor* clipboard_permission =
          NativeValueTraits<ClipboardPermissionDescriptor>::NativeValue(
              script_state->GetIsolate(), raw_descriptor.V8Value(),
              exception_state);
      return CreateClipboardPermissionDescriptor(
          PermissionName::CLIPBOARD_READ,
          /*has_user_gesture=*/!clipboard_permission->allowWithoutGesture(),
          /*will_be_sanitized=*/
          !clipboard_permission->allowWithoutSanitization());
    }

    case V8PermissionName::Enum::kClipboardWrite: {
      ClipboardPermissionDescriptor* clipboard_permission =
          NativeValueTraits<ClipboardPermissionDescriptor>::NativeValue(
              script_state->GetIsolate(), raw_descriptor.V8Value(),
              exception_state);
      return CreateClipboardPermissionDescriptor(
          PermissionName::CLIPBOARD_WRITE,
          /*has_user_gesture=*/!clipboard_permission->allowWithoutGesture(),
          /*will_be_sanitized=*/
          !clipboard_permission->allowWithoutSanitization());
    }

    case V8PermissionName::Enum::kPaymentHandler:
      return CreatePermissionDescriptor(PermissionName::PAYMENT_HANDLER);

    case V8PermissionName::Enum::kIdleDetection:
      return CreatePermissionDescriptor(PermissionName::IDLE_DETECTION);

    case V8PermissionName::Enum::kPeriodicBackgroundSync:
      return CreatePermissionDescriptor(
          PermissionName::PERIODIC_BACKGROUND_SYNC);

    case V8PermissionName::Enum::kSystemWakeLock: {
      if (!RuntimeEnabledFeatures::SystemWakeLockEnabled(
              ExecutionContext::From(script_state))) {
        exception_state.ThrowTypeError("System Wake Lock is not enabled.");
        return nullptr;
      }
      return CreatePermissionDescriptor(PermissionName::SYSTEM_WAKE_LOCK);
    }

    case V8PermissionName::Enum::kStorageAccess:
      return CreatePermissionDescriptor(PermissionName::STORAGE_ACCESS);

    case V8PermissionName::Enum::kWindowManagement:
      return CreatePermissionDescriptor(PermissionName::WINDOW_MANAGEMENT);

    case V8PermissionName::Enum::kLocalFonts: {
      if (!RuntimeEnabledFeatures::FontAccessEnabled(
              ExecutionContext::From(script_state))) {
        exception_state.ThrowTypeError(
            "Local Fonts Access API is not enabled.");
        return nullptr;
      }
      return CreatePermissionDescriptor(PermissionName::LOCAL_FONTS);
    }

    case V8PermissionName::Enum::kTopLevelStorageAccess: {
      TopLevelStorageAccessPermissionDescriptor*
          top_level_storage_access_permission =
              NativeValueTraits<TopLevelStorageAccessPermissionDescriptor>::
                  NativeValue(script_state->GetIsolate(),
                              raw_descriptor.V8Value(), exception_state);
      if (exception_state.HadException()) {
        return nullptr;
      }
      KURL origin_as_kurl{
          top_level_storage_access_permission->requestedOrigin()};
      if (!origin_as_kurl.IsValid()) {
        exception_state.ThrowTypeError("The requested origin is invalid.");
        return nullptr;
      }

      return CreateTopLevelStorageAccessPermissionDescriptor(origin_as_kurl);
    }

    case V8PermissionName::Enum::kCapturedSurfaceControl: {
      if (!RuntimeEnabledFeatures::CapturedSurfaceControlEnabled(
              ExecutionContext::From(script_state))) {
        exception_state.ThrowTypeError(
            "The Captured Surface Control API is not enabled.");
        return nullptr;
      }
      return CreatePermissionDescriptor(
          PermissionName::CAPTURED_SURFACE_CONTROL);
    }

    case V8PermissionName::Enum::kSpeakerSelection: {
      if (!RuntimeEnabledFeatures::SpeakerSelectionEnabled(
              ExecutionContext::From(script_state))) {
        exception_state.ThrowTypeError(
            "The Speaker Selection API is not enabled.");
        return nullptr;
      }
      return CreatePermissionDescriptor(PermissionName::SPEAKER_SELECTION);
    }

    case V8PermissionName::Enum::kKeyboardLock: {
#if !BUILDFLAG(IS_ANDROID)
      return CreatePermissionDescriptor(PermissionName::KEYBOARD_LOCK);
#else
      exception_state.ThrowTypeError(
          "The Keyboard Lock permission isn't available on Android.");
      return nullptr;
#endif
    }

    case V8PermissionName::Enum::kPointerLock: {
#if !BUILDFLAG(IS_ANDROID)
      return CreatePermissionDescriptor(PermissionName::POINTER_LOCK);
#else
      exception_state.ThrowTypeError(
          "The Pointer Lock permission isn't available on Android.");
      return nullptr;
#endif
    }

    case V8PermissionName::Enum::kFullscreen: {
      FullscreenPermissionDescriptor* fullscreen_permission =
          NativeValueTraits<FullscreenPermissionDescriptor>::NativeValue(
              script_state->GetIsolate(), raw_descriptor.V8Value(),
              exception_state);
      if (exception_state.HadException()) {
        return nullptr;
      }
      if (!fullscreen_permission->allowWithoutGesture()) {
        // There is no permission state for fullscreen with user gesture.
        exception_state.ThrowTypeError(
            "Fullscreen Permission only supports allowWithoutGesture:true.");
        return nullptr;
      }
      return CreateFullscreenPermissionDescriptor(
          fullscreen_permission->allowWithoutGesture());
    }

    case V8PermissionName::Enum::kWebAppInstallation: {
      if (!RuntimeEnabledFeatures::WebAppInstallationEnabled(
              ExecutionContext::From(script_state))) {
        exception_state.ThrowTypeError(
            "The Web App Install API is not enabled.");
        return nullptr;
      }
      return CreatePermissionDescriptor(PermissionName::WEB_APP_INSTALLATION);
    }

    case V8PermissionName::Enum::kLocalNetworkAccess:
      return CreatePermissionDescriptor(PermissionName::LOCAL_NETWORK_ACCESS);

    case V8PermissionName::Enum::kLocalNetwork: {
      if (!RuntimeEnabledFeatures::
              LocalNetworkAccessSplitPermissionsEnabled()) {
        exception_state.ThrowTypeError(
            "Local Network Access Split permissions are not enabled.");
        return nullptr;
      }
      return CreatePermissionDescriptor(PermissionName::LOCAL_NETWORK);
    }

    case V8PermissionName::Enum::kLoopbackNetwork: {
      if (!RuntimeEnabledFeatures::
              LocalNetworkAccessSplitPermissionsEnabled()) {
        exception_state.ThrowTypeError(
            "Local Network Access Split permissions are not enabled.");
        return nullptr;
      }
      return CreatePermissionDescriptor(PermissionName::LOOPBACK_NETWORK);
    }
  }

  return nullptr;
}

}  // namespace blink
