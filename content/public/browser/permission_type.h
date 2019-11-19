// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_TYPE_H_

#include <vector>

#include "content/common/content_export.h"

namespace content {

// This enum is also used for UMA purposes, so it needs to adhere to
// the UMA guidelines.
// Make sure you update enums.xml and GetAllPermissionTypes if you add
// new permission types.
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
  FLASH = 11,
  SENSORS = 12,
  ACCESSIBILITY_EVENTS = 13,
  CLIPBOARD_READ = 14,
  CLIPBOARD_WRITE = 15,
  PAYMENT_HANDLER = 16,
  BACKGROUND_FETCH = 17,
  IDLE_DETECTION = 18,
  PERIODIC_BACKGROUND_SYNC = 19,
  WAKE_LOCK_SCREEN = 20,
  WAKE_LOCK_SYSTEM = 21,
  NFC = 22,

  // Always keep this at the end.
  NUM,
};

CONTENT_EXPORT const std::vector<PermissionType>& GetAllPermissionTypes();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_TYPE_H_
