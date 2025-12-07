// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_descriptor_util.h"

namespace {
static blink::mojom::PermissionDescriptorPtr CreatePermissionDescriptor(
    blink::mojom::PermissionName name) {
  auto descriptor = blink::mojom::PermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

static blink::mojom::PermissionDescriptorPtr CreateMidiPermissionDescriptor(
    bool sysex) {
  auto descriptor =
      CreatePermissionDescriptor(blink::mojom::PermissionName::MIDI);
  auto midi_extension = blink::mojom::MidiPermissionDescriptor::New();
  midi_extension->sysex = sysex;
  descriptor->extension = blink::mojom::PermissionDescriptorExtension::NewMidi(
      std::move(midi_extension));
  return descriptor;
}

static blink::mojom::PermissionDescriptorPtr
CreateClipboardPermissionDescriptor(blink::mojom::PermissionName name,
                                    bool has_user_gesture,
                                    bool will_be_sanitized) {
  auto descriptor = CreatePermissionDescriptor(name);
  auto clipboard_extension = blink::mojom::ClipboardPermissionDescriptor::New(
      has_user_gesture, will_be_sanitized);
  descriptor->extension =
      blink::mojom::PermissionDescriptorExtension::NewClipboard(
          std::move(clipboard_extension));
  return descriptor;
}

static blink::mojom::PermissionDescriptorPtr
CreateVideoCapturePermissionDescriptor(bool pan_tilt_zoom) {
  auto descriptor =
      CreatePermissionDescriptor(blink::mojom::PermissionName::VIDEO_CAPTURE);
  auto camera_device_extension =
      blink::mojom::CameraDevicePermissionDescriptor::New(pan_tilt_zoom);
  descriptor->extension =
      blink::mojom::PermissionDescriptorExtension::NewCameraDevice(
          std::move(camera_device_extension));
  return descriptor;
}

static blink::mojom::PermissionDescriptorPtr
CreateFullscreenPermissionDescriptor(bool allow_without_user_gesture) {
  auto descriptor =
      CreatePermissionDescriptor(blink::mojom::PermissionName::FULLSCREEN);
  auto fullscreen_extension = blink::mojom::FullscreenPermissionDescriptor::New(
      allow_without_user_gesture);
  descriptor->extension =
      blink::mojom::PermissionDescriptorExtension::NewFullscreen(
          std::move(fullscreen_extension));
  return descriptor;
}
}  // namespace

namespace content {

// static
blink::mojom::PermissionDescriptorPtr
content::PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionType(
    blink::PermissionType permission_type) {
  switch (permission_type) {
    case blink::PermissionType::MIDI_SYSEX:
      return CreateMidiPermissionDescriptor(/*sysex=*/true);
    case blink::PermissionType::NOTIFICATIONS:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::NOTIFICATIONS);
    case blink::PermissionType::GEOLOCATION:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::GEOLOCATION);
    case blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::PROTECTED_MEDIA_IDENTIFIER);
    case blink::PermissionType::MIDI:
      return CreateMidiPermissionDescriptor(false);
    case blink::PermissionType::DURABLE_STORAGE:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::DURABLE_STORAGE);
    case blink::PermissionType::AUDIO_CAPTURE:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::AUDIO_CAPTURE);
    case blink::PermissionType::VIDEO_CAPTURE:
      return CreateVideoCapturePermissionDescriptor(/*pan_tilt_zoom=*/false);
    case blink::PermissionType::BACKGROUND_SYNC:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::BACKGROUND_SYNC);
    case blink::PermissionType::SENSORS:
      return CreatePermissionDescriptor(blink::mojom::PermissionName::SENSORS);
    case blink::PermissionType::PAYMENT_HANDLER:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::PAYMENT_HANDLER);
    case blink::PermissionType::BACKGROUND_FETCH:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::BACKGROUND_FETCH);
    case blink::PermissionType::IDLE_DETECTION:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::IDLE_DETECTION);
    case blink::PermissionType::PERIODIC_BACKGROUND_SYNC:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::PERIODIC_BACKGROUND_SYNC);
    case blink::PermissionType::WAKE_LOCK_SCREEN:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::SCREEN_WAKE_LOCK);
    case blink::PermissionType::WAKE_LOCK_SYSTEM:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::SYSTEM_WAKE_LOCK);
    case blink::PermissionType::NFC:
      return CreatePermissionDescriptor(blink::mojom::PermissionName::NFC);
    case blink::PermissionType::CLIPBOARD_READ_WRITE:
      return CreateClipboardPermissionDescriptor(
          blink::mojom::PermissionName::CLIPBOARD_WRITE, false, false);
    case blink::PermissionType::CLIPBOARD_SANITIZED_WRITE:
      return CreateClipboardPermissionDescriptor(
          blink::mojom::PermissionName::CLIPBOARD_WRITE, true, true);
    case blink::PermissionType::VR:
      return CreatePermissionDescriptor(blink::mojom::PermissionName::VR);
    case blink::PermissionType::AR:
      return CreatePermissionDescriptor(blink::mojom::PermissionName::AR);
    case blink::PermissionType::STORAGE_ACCESS_GRANT:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::STORAGE_ACCESS);
    case blink::PermissionType::CAMERA_PAN_TILT_ZOOM:
      return CreateVideoCapturePermissionDescriptor(true);
    case blink::PermissionType::WINDOW_MANAGEMENT:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::WINDOW_MANAGEMENT);
    case blink::PermissionType::LOCAL_FONTS:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::LOCAL_FONTS);
    case blink::PermissionType::DISPLAY_CAPTURE:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::DISPLAY_CAPTURE);
    case blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::TOP_LEVEL_STORAGE_ACCESS);
    case blink::PermissionType::CAPTURED_SURFACE_CONTROL:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::CAPTURED_SURFACE_CONTROL);
    case blink::PermissionType::SMART_CARD:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::SMART_CARD);
    case blink::PermissionType::WEB_PRINTING:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::WEB_PRINTING);
    case blink::PermissionType::SPEAKER_SELECTION:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::SPEAKER_SELECTION);
    case blink::PermissionType::KEYBOARD_LOCK:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::KEYBOARD_LOCK);
    case blink::PermissionType::POINTER_LOCK:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::POINTER_LOCK);
    case blink::PermissionType::AUTOMATIC_FULLSCREEN:
      return CreateFullscreenPermissionDescriptor(
          /*allow_without_user_gesture=*/true);
    case blink::PermissionType::HAND_TRACKING:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::HAND_TRACKING);
    case blink::PermissionType::WEB_APP_INSTALLATION:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::WEB_APP_INSTALLATION);
    case blink::PermissionType::LOCAL_NETWORK_ACCESS:
      return CreatePermissionDescriptor(
          blink::mojom::PermissionName::LOCAL_NETWORK_ACCESS);
    case blink::PermissionType::NUM:
      NOTREACHED();
  }
  NOTREACHED();
}

// static
std::vector<blink::mojom::PermissionDescriptorPtr>
PermissionDescriptorUtil::CreatePermissionDescriptorForPermissionTypes(
    const std::vector<blink::PermissionType>& permission_types) {
  std::vector<blink::mojom::PermissionDescriptorPtr> descriptors;
  descriptors.reserve(permission_types.size());
  for (const auto& permission_type : permission_types) {
    descriptors.emplace_back(
        CreatePermissionDescriptorForPermissionType(permission_type));
  }
  return descriptors;
}

}  // namespace content
