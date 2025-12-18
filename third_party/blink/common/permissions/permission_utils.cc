// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions/permission_utils.h"

#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

namespace blink {

using mojom::PermissionDescriptorPtr;
using mojom::PermissionName;
using mojom::PermissionStatus;

mojom::PermissionStatus ToPermissionStatus(const std::string& status) {
  if (status == "granted")
    return mojom::PermissionStatus::GRANTED;
  if (status == "prompt")
    return mojom::PermissionStatus::ASK;
  if (status == "denied")
    return mojom::PermissionStatus::DENIED;
  NOTREACHED();
}

std::string GetPermissionString(PermissionType permission) {
  switch (permission) {
    case PermissionType::GEOLOCATION:
      return "Geolocation";
    case PermissionType::GEOLOCATION_APPROXIMATE:
      return "GeolocationApproximate";
    case PermissionType::NOTIFICATIONS:
      return "Notifications";
    case PermissionType::MIDI_SYSEX:
      return "MidiSysEx";
    case PermissionType::DURABLE_STORAGE:
      return "DurableStorage";
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return "ProtectedMediaIdentifier";
    case PermissionType::AUDIO_CAPTURE:
      return "AudioCapture";
    case PermissionType::VIDEO_CAPTURE:
      return "VideoCapture";
    case PermissionType::MIDI:
      return "Midi";
    case PermissionType::BACKGROUND_SYNC:
      return "BackgroundSync";
    case PermissionType::SENSORS:
      return "Sensors";
    case PermissionType::CLIPBOARD_READ_WRITE:
      return "ClipboardReadWrite";
    case PermissionType::CLIPBOARD_SANITIZED_WRITE:
      return "ClipboardSanitizedWrite";
    case PermissionType::PAYMENT_HANDLER:
      return "PaymentHandler";
    case PermissionType::BACKGROUND_FETCH:
      return "BackgroundFetch";
    case PermissionType::IDLE_DETECTION:
      return "IdleDetection";
    case PermissionType::PERIODIC_BACKGROUND_SYNC:
      return "PeriodicBackgroundSync";
    case PermissionType::WAKE_LOCK_SCREEN:
      return "WakeLockScreen";
    case PermissionType::WAKE_LOCK_SYSTEM:
      return "WakeLockSystem";
    case PermissionType::NFC:
      return "NFC";
    case PermissionType::VR:
      return "VR";
    case PermissionType::AR:
      return "AR";
    case PermissionType::HAND_TRACKING:
      return "HandTracking";
    case PermissionType::SMART_CARD:
      return "SmartCard";
    case PermissionType::STORAGE_ACCESS_GRANT:
      return "StorageAccess";
    case PermissionType::CAMERA_PAN_TILT_ZOOM:
      return "CameraPanTiltZoom";
    case PermissionType::WINDOW_MANAGEMENT:
      return "WindowManagement";
    case PermissionType::LOCAL_FONTS:
      return "LocalFonts";
    case PermissionType::DISPLAY_CAPTURE:
      return "DisplayCapture";
    case PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      return "TopLevelStorageAccess";
    case PermissionType::CAPTURED_SURFACE_CONTROL:
      return "CapturedSurfaceControl";
    case PermissionType::WEB_PRINTING:
      return "WebPrinting";
    case PermissionType::SPEAKER_SELECTION:
      return "SpeakerSelection";
    case PermissionType::KEYBOARD_LOCK:
      return "KeyboardLock";
    case PermissionType::POINTER_LOCK:
      return "PointerLock";
    case PermissionType::AUTOMATIC_FULLSCREEN:
      return "AutomaticFullscreen";
    case PermissionType::WEB_APP_INSTALLATION:
      return "WebAppInstallation";
    case PermissionType::LOCAL_NETWORK_ACCESS:
      return "LocalNetworkAccess";
    case PermissionType::LOCAL_NETWORK:
      return "LocalNetwork";
    case PermissionType::LOOPBACK_NETWORK:
      return "LoopbackNetwork";
    case PermissionType::NUM:
      NOTREACHED();
  }
  NOTREACHED();
}

std::optional<network::mojom::PermissionsPolicyFeature>
PermissionTypeToPermissionsPolicyFeature(PermissionType permission) {
  switch (permission) {
    case PermissionType::GEOLOCATION:
    // TODO(crbug.com/465377576): Change this when we implement the new
    // policy-controlled feature "geolocation-approximate".
    case PermissionType::GEOLOCATION_APPROXIMATE:
      return network::mojom::PermissionsPolicyFeature::kGeolocation;
    case PermissionType::MIDI_SYSEX:
      return network::mojom::PermissionsPolicyFeature::kMidiFeature;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return network::mojom::PermissionsPolicyFeature::kEncryptedMedia;
    case PermissionType::AUDIO_CAPTURE:
      return network::mojom::PermissionsPolicyFeature::kMicrophone;
    case PermissionType::VIDEO_CAPTURE:
      return network::mojom::PermissionsPolicyFeature::kCamera;
    case PermissionType::MIDI:
      return network::mojom::PermissionsPolicyFeature::kMidiFeature;
    case PermissionType::CLIPBOARD_READ_WRITE:
      return network::mojom::PermissionsPolicyFeature::kClipboardRead;
    case PermissionType::CLIPBOARD_SANITIZED_WRITE:
      return network::mojom::PermissionsPolicyFeature::kClipboardWrite;
    case PermissionType::IDLE_DETECTION:
      return network::mojom::PermissionsPolicyFeature::kIdleDetection;
    case PermissionType::WAKE_LOCK_SCREEN:
      return network::mojom::PermissionsPolicyFeature::kScreenWakeLock;
    case PermissionType::HAND_TRACKING:
      return network::mojom::PermissionsPolicyFeature::kWebXr;
    case PermissionType::VR:
      return network::mojom::PermissionsPolicyFeature::kWebXr;
    case PermissionType::AR:
      return network::mojom::PermissionsPolicyFeature::kWebXr;
    case PermissionType::SMART_CARD:
      return network::mojom::PermissionsPolicyFeature::kSmartCard;
    case PermissionType::WEB_PRINTING:
      return network::mojom::PermissionsPolicyFeature::kWebPrinting;
    case PermissionType::STORAGE_ACCESS_GRANT:
      return network::mojom::PermissionsPolicyFeature::kStorageAccessAPI;
    case PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      return network::mojom::PermissionsPolicyFeature::kStorageAccessAPI;
    case PermissionType::WINDOW_MANAGEMENT:
      return network::mojom::PermissionsPolicyFeature::kWindowManagement;
    case PermissionType::LOCAL_FONTS:
      return network::mojom::PermissionsPolicyFeature::kLocalFonts;
    case PermissionType::DISPLAY_CAPTURE:
      return network::mojom::PermissionsPolicyFeature::kDisplayCapture;
    case PermissionType::CAPTURED_SURFACE_CONTROL:
      return network::mojom::PermissionsPolicyFeature::kCapturedSurfaceControl;
    case PermissionType::SPEAKER_SELECTION:
      return network::mojom::PermissionsPolicyFeature::kSpeakerSelection;
    case PermissionType::AUTOMATIC_FULLSCREEN:
      return network::mojom::PermissionsPolicyFeature::kFullscreen;
    case PermissionType::WEB_APP_INSTALLATION:
      return network::mojom::PermissionsPolicyFeature::kWebAppInstallation;
    case PermissionType::LOCAL_NETWORK_ACCESS:
      return network::mojom::PermissionsPolicyFeature::kLocalNetworkAccess;
    case PermissionType::LOCAL_NETWORK:
      return network::mojom::PermissionsPolicyFeature::kLocalNetwork;
    case PermissionType::LOOPBACK_NETWORK:
      return network::mojom::PermissionsPolicyFeature::kLoopbackNetwork;

    case PermissionType::PERIODIC_BACKGROUND_SYNC:
    case PermissionType::DURABLE_STORAGE:
    case PermissionType::BACKGROUND_SYNC:
    // TODO(crbug.com/1384434): decouple this to separated types of sensor,
    // with a corresponding permission policy.
    case PermissionType::SENSORS:
    case PermissionType::PAYMENT_HANDLER:
    case PermissionType::BACKGROUND_FETCH:
    case PermissionType::WAKE_LOCK_SYSTEM:
    case PermissionType::NFC:
    case PermissionType::CAMERA_PAN_TILT_ZOOM:
    case PermissionType::NOTIFICATIONS:
    case PermissionType::KEYBOARD_LOCK:
    case PermissionType::POINTER_LOCK:
      return std::nullopt;

    case PermissionType::NUM:
      NOTREACHED();
  }
  NOTREACHED();
}

const std::vector<PermissionType>& GetAllPermissionTypes() {
  static const base::NoDestructor<std::vector<PermissionType>>
      kAllPermissionTypes([] {
        const int NUM_TYPES = static_cast<int>(PermissionType::NUM);
        std::vector<PermissionType> all_types;
        // Note: Update this if the set of removed entries changes.
        // This is 7 because it skips 0 as well as the 6 numbers explicitly
        // mentioned below.
        all_types.reserve(NUM_TYPES - 7);
        for (int i = 1; i < NUM_TYPES; ++i) {
          // Skip removed entries.
          if (i == 2 || i == 11 || i == 13 || i == 14 || i == 15 || i == 32) {
            continue;
          }
          all_types.push_back(static_cast<PermissionType>(i));
        }
        return all_types;
      }());
  return *kAllPermissionTypes;
}

std::optional<PermissionType> MaybePermissionDescriptorToPermissionType(
    const PermissionDescriptorPtr& descriptor) {
  return PermissionDescriptorInfoToPermissionType(
      descriptor->name,
      descriptor->extension && descriptor->extension->is_midi() &&
          descriptor->extension->get_midi()->sysex,
      descriptor->extension && descriptor->extension->is_camera_device() &&
          descriptor->extension->get_camera_device()->panTiltZoom,
      descriptor->extension && descriptor->extension->is_clipboard() &&
          descriptor->extension->get_clipboard()->will_be_sanitized,
      descriptor->extension && descriptor->extension->is_clipboard() &&
          descriptor->extension->get_clipboard()->has_user_gesture,
      descriptor->extension && descriptor->extension->is_fullscreen() &&
          descriptor->extension->get_fullscreen()->allow_without_user_gesture);
}

PermissionType PermissionDescriptorToPermissionType(
    const PermissionDescriptorPtr& descriptor) {
  auto permission_type_optional =
      MaybePermissionDescriptorToPermissionType(descriptor);
  CHECK(permission_type_optional.has_value());
  return permission_type_optional.value();
}

std::vector<PermissionType> PermissionDescriptorToPermissionTypes(
    const std::vector<PermissionDescriptorPtr>& descriptors) {
  std::vector<PermissionType> permission_types;
  permission_types.reserve(descriptors.size());

  for (const auto& descriptor : descriptors) {
    permission_types.emplace_back(
        PermissionDescriptorToPermissionType(descriptor));
  }

  return permission_types;
}

std::optional<PermissionType> PermissionDescriptorInfoToPermissionType(
    mojom::PermissionName name,
    bool midi_sysex,
    bool camera_ptz,
    bool clipboard_will_be_sanitized,
    bool clipboard_has_user_gesture,
    bool fullscreen_allow_without_user_gesture) {
  switch (name) {
    case PermissionName::GEOLOCATION:
      return PermissionType::GEOLOCATION;
    case PermissionName::GEOLOCATION_APPROXIMATE:
      return PermissionType::GEOLOCATION_APPROXIMATE;
    case PermissionName::NOTIFICATIONS:
      return PermissionType::NOTIFICATIONS;
    case PermissionName::MIDI: {
      if (midi_sysex) {
        return PermissionType::MIDI_SYSEX;
      }
      return PermissionType::MIDI;
    }
    case PermissionName::PROTECTED_MEDIA_IDENTIFIER:
#if defined(ENABLE_PROTECTED_MEDIA_IDENTIFIER_PERMISSION)
      return PermissionType::PROTECTED_MEDIA_IDENTIFIER;
#else
      NOTIMPLEMENTED();
      return std::nullopt;
#endif  // defined(ENABLE_PROTECTED_MEDIA_IDENTIFIER_PERMISSION)
    case PermissionName::DURABLE_STORAGE:
      return PermissionType::DURABLE_STORAGE;
    case PermissionName::AUDIO_CAPTURE:
      return PermissionType::AUDIO_CAPTURE;
    case PermissionName::VIDEO_CAPTURE:
      if (camera_ptz) {
        return PermissionType::CAMERA_PAN_TILT_ZOOM;
      } else {
        return PermissionType::VIDEO_CAPTURE;
      }
    case PermissionName::BACKGROUND_SYNC:
      return PermissionType::BACKGROUND_SYNC;
    case PermissionName::SENSORS:
      return PermissionType::SENSORS;
    case PermissionName::CLIPBOARD_READ:
      return PermissionType::CLIPBOARD_READ_WRITE;
    case PermissionName::CLIPBOARD_WRITE:
      // If the write is both sanitized (i.e. plain text or known-format
      // images), and a user gesture is present, use CLIPBOARD_SANITIZED_WRITE,
      // which Chrome grants by default.
      if (clipboard_will_be_sanitized && clipboard_has_user_gesture) {
        return PermissionType::CLIPBOARD_SANITIZED_WRITE;
      } else {
        return PermissionType::CLIPBOARD_READ_WRITE;
      }
    case PermissionName::PAYMENT_HANDLER:
      return PermissionType::PAYMENT_HANDLER;
    case PermissionName::BACKGROUND_FETCH:
      return PermissionType::BACKGROUND_FETCH;
    case PermissionName::IDLE_DETECTION:
      return PermissionType::IDLE_DETECTION;
    case PermissionName::PERIODIC_BACKGROUND_SYNC:
      return PermissionType::PERIODIC_BACKGROUND_SYNC;
    case PermissionName::SCREEN_WAKE_LOCK:
      return PermissionType::WAKE_LOCK_SCREEN;
    case PermissionName::SYSTEM_WAKE_LOCK:
      return PermissionType::WAKE_LOCK_SYSTEM;
    case PermissionName::NFC:
      return PermissionType::NFC;
    case PermissionName::STORAGE_ACCESS:
      return PermissionType::STORAGE_ACCESS_GRANT;
    case PermissionName::WINDOW_MANAGEMENT:
      return PermissionType::WINDOW_MANAGEMENT;
    case PermissionName::LOCAL_FONTS:
      return PermissionType::LOCAL_FONTS;
    case PermissionName::DISPLAY_CAPTURE:
      return PermissionType::DISPLAY_CAPTURE;
    case PermissionName::TOP_LEVEL_STORAGE_ACCESS:
      return PermissionType::TOP_LEVEL_STORAGE_ACCESS;
    case PermissionName::CAPTURED_SURFACE_CONTROL:
      return PermissionType::CAPTURED_SURFACE_CONTROL;
    case PermissionName::SPEAKER_SELECTION:
      return PermissionType::SPEAKER_SELECTION;
    case PermissionName::KEYBOARD_LOCK:
      return PermissionType::KEYBOARD_LOCK;
    case PermissionName::POINTER_LOCK:
      return PermissionType::POINTER_LOCK;
    case PermissionName::FULLSCREEN:
      if (fullscreen_allow_without_user_gesture) {
        return PermissionType::AUTOMATIC_FULLSCREEN;
      }
      // There is no PermissionType for fullscreen with user gesture.
      NOTIMPLEMENTED_LOG_ONCE();
      return std::nullopt;
    case PermissionName::WEB_APP_INSTALLATION:
      return PermissionType::WEB_APP_INSTALLATION;
    case PermissionName::LOCAL_NETWORK_ACCESS:
      return PermissionType::LOCAL_NETWORK_ACCESS;
    case PermissionName::LOCAL_NETWORK:
      return PermissionType::LOCAL_NETWORK;
    case PermissionName::LOOPBACK_NETWORK:
      return PermissionType::LOOPBACK_NETWORK;
    case PermissionName::VR:
      return PermissionType::VR;
    case PermissionName::AR:
      return PermissionType::AR;
    case PermissionName::HAND_TRACKING:
      return PermissionType::HAND_TRACKING;
    case PermissionName::WEB_PRINTING:
      return PermissionType::WEB_PRINTING;
    case PermissionName::SMART_CARD:
      return PermissionType::SMART_CARD;
  }
}

}  // namespace blink
