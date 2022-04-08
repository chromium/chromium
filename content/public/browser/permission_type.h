// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_TYPE_H_

#include <vector>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"

namespace content {

// This enum is also used for UMA purposes, so it needs to adhere to
// the UMA guidelines.
// Make sure you update enums.xml and GetAllPermissionTypes if you add new
// or deprecate permission types.
// Never delete or reorder an entry; only add new entries
// immediately before PermissionType::NUM
enum class PermissionType {
  MIDI_SYSEX = 1,
  // PUSH_MESSAGING = 2,
  NOTIFICATIONS = 3,
  GEOLOCATION = 4,
  PROTECTED_MEDIA_IDENTIFIER = 5,
  MIDI = 6,
  DURABLE_STORAGE = 7,
  AUDIO_CAPTURE = 8,
  VIDEO_CAPTURE = 9,
  BACKGROUND_SYNC = 10,
  // FLASH = 11,
  SENSORS = 12,
  ACCESSIBILITY_EVENTS = 13,
  // CLIPBOARD_READ = 14, // Replaced by CLIPBOARD_READ_WRITE in M81.
  // CLIPBOARD_WRITE = 15, // Replaced by CLIPBOARD_SANITIZED_WRITE in M81.
  PAYMENT_HANDLER = 16,
  BACKGROUND_FETCH = 17,
  IDLE_DETECTION = 18,
  PERIODIC_BACKGROUND_SYNC = 19,
  WAKE_LOCK_SCREEN = 20,
  WAKE_LOCK_SYSTEM = 21,
  NFC = 22,
  CLIPBOARD_READ_WRITE = 23,
  CLIPBOARD_SANITIZED_WRITE = 24,
  VR = 25,
  AR = 26,
  STORAGE_ACCESS_GRANT = 27,
  CAMERA_PAN_TILT_ZOOM = 28,
  WINDOW_PLACEMENT = 29,
  LOCAL_FONTS = 30,
  DISPLAY_CAPTURE = 31,
  // FILE_HANDLING = 32,  // Removed in M98.

  // Always keep this at the end.
  NUM,
};

CONTENT_EXPORT const std::vector<PermissionType>& GetAllPermissionTypes();

// Given |descriptor|, set |permission_type| to a corresponding PermissionType.
CONTENT_EXPORT absl::optional<PermissionType>
PermissionDescriptorToPermissionType(
    const blink::mojom::PermissionDescriptorPtr& descriptor);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_TYPE_H_
