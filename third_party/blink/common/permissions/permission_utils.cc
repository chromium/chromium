// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions/permission_utils.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
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
  return mojom::PermissionStatus::DENIED;
}

const std::vector<PermissionType>& GetAllPermissionTypes() {
  static const base::NoDestructor<std::vector<PermissionType>>
      kAllPermissionTypes([] {
        const int NUM_TYPES = static_cast<int>(PermissionType::NUM);
        std::vector<PermissionType> all_types;
        // Note: Update this if the set of removed entries changes.
        // This is 6 because it skips 0 as well as the 5 numbers explicitly
        // mentioned below.
        all_types.reserve(NUM_TYPES - 6);
        for (int i = 1; i < NUM_TYPES; ++i) {
          // Skip removed entries.
          if (i == 2 || i == 11 || i == 14 || i == 15 || i == 32)
            continue;
          all_types.push_back(static_cast<PermissionType>(i));
        }
        return all_types;
      }());
  return *kAllPermissionTypes;
}

absl::optional<PermissionType> PermissionDescriptorToPermissionType(
    const PermissionDescriptorPtr& descriptor) {
  return PermissionDescriptorInfoToPermissionType(
      descriptor->name,
      descriptor->extension && descriptor->extension->is_midi() &&
          descriptor->extension->get_midi()->sysex,
      descriptor->extension && descriptor->extension->is_camera_device() &&
          descriptor->extension->get_camera_device()->panTiltZoom,
      descriptor->extension && descriptor->extension->is_clipboard() &&
          descriptor->extension->get_clipboard()->allowWithoutSanitization);
}

absl::optional<PermissionType> PermissionDescriptorInfoToPermissionType(
    mojom::PermissionName name,
    bool midi_sysex,
    bool camera_ptz,
    bool clipboard_allow_without_sanitization) {
  switch (name) {
    case PermissionName::GEOLOCATION:
      return PermissionType::GEOLOCATION;
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
      return absl::nullopt;
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
    case PermissionName::ACCESSIBILITY_EVENTS:
      return PermissionType::ACCESSIBILITY_EVENTS;
    case PermissionName::CLIPBOARD_READ:
      return PermissionType::CLIPBOARD_READ_WRITE;
    case PermissionName::CLIPBOARD_WRITE: {
      if (clipboard_allow_without_sanitization) {
        return PermissionType::CLIPBOARD_READ_WRITE;
      } else {
        return PermissionType::CLIPBOARD_SANITIZED_WRITE;
      }
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
    case PermissionName::WINDOW_PLACEMENT:
      return PermissionType::WINDOW_PLACEMENT;
    case PermissionName::LOCAL_FONTS:
      return PermissionType::LOCAL_FONTS;
    case PermissionName::DISPLAY_CAPTURE:
      return PermissionType::DISPLAY_CAPTURE;

      NOTREACHED();
      return absl::nullopt;
  }
}

}  // namespace blink
