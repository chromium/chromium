// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_WAYLAND_MANAGER_H_
#define REMOTING_HOST_LINUX_WAYLAND_MANAGER_H_

#include <memory>

#include <xkbcommon/xkbcommon.h>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/wayland_connection.h"
#include "remoting/host/linux/wayland_display.h"
#include "remoting/host/linux/wayland_seat.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_metadata.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

// Helper class that facilitates interaction of different Wayland related
// components under chromoting.
class WaylandManager {
 public:
  using DesktopMetadataCallbackSignature = void(webrtc::DesktopCaptureMetadata);
  using DesktopMetadataCallback =
      base::RepeatingCallback<DesktopMetadataCallbackSignature>;
  using UpdateScreenResolutionSignature = void(ScreenResolution,
                                               webrtc::ScreenId);
  using UpdateScreenResolutionCallback =
      base::RepeatingCallback<UpdateScreenResolutionSignature>;
  using KeyboardLayoutCallback =
      base::RepeatingCallback<void(XkbKeyMapUniquePtr)>;
  using KeyboardModifiersCallbackSignature = void(uint32_t group);
  using KeyboardModifiersCallback =
      base::RepeatingCallback<KeyboardModifiersCallbackSignature>;
  using ClipboardMetadataCallbackSignature =
      void(webrtc::DesktopCaptureMetadata);
  using ClipboardMetadataCallback =
      base::RepeatingCallback<ClipboardMetadataCallbackSignature>;

  WaylandManager();
  ~WaylandManager();
  WaylandManager(const WaylandManager&) = delete;
  WaylandManager& operator=(const WaylandManager&) = delete;

  static WaylandManager* Get();

  // Cleans up reference to runner. (Needed only for testing)
  void CleanupRunnerForTest();

  // The singleton instance should be initialized by the host process on the
  // UI thread right after creation.
  void Init(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  // Adds callback to be invoked when a desktop capturer has metadata available.
  void AddCapturerMetadataCallback(DesktopMetadataCallback callback);

  // Adds callback to be invoked when a desktop capturer is destroyed.
  // TODO(crbug.com/40266740): This would need to be enhanced when supporting
  // multiple desktops/capturers.
  void AddCapturerDestroyedCallback(base::OnceClosure callback);

  // Invoked by the desktop capturer(s), upon successful start.
  void OnDesktopCapturerMetadata(webrtc::DesktopCaptureMetadata metadata);

  // Invoked by the desktop capturer(s), upon destruction.
  // TODO(crbug.com/40266740): This would need to be enhanced when supporting
  // multiple desktops/capturers and is likely going to notify the listener only
  // when the last desktop capturer is destroyed.
  void OnDesktopCapturerDestroyed();

  // Adds callback to be invoked when clipboard has metadata available.
  void AddClipboardMetadataCallback(DesktopMetadataCallback callback);

  // Invoked by the clipboard portal upon a successful start.
  void OnClipboardMetadata(webrtc::DesktopCaptureMetadata metadata);

  // Adds callback to be invoked when screen resolution is updated by the
  // desktop resizer.
  void AddUpdateScreenResolutionCallback(
      UpdateScreenResolutionCallback callback);

  // Invoked by the desktop_resizer_wayland upon screen resolution update from
  // resizing_host_observer.
  void OnUpdateScreenResolution(ScreenResolution resolution,
                                webrtc::ScreenId screen_id);

  // Sets callback to be invoked when new keyboard layout is detected.
  void SetKeyboardLayoutCallback(KeyboardLayoutCallback callback);

  // Invoked by the wayland keyboard, upon detecting a new keyboard layout
  // mapping from the compositor.
  void OnKeyboardLayout(XkbKeyMapUniquePtr);

  // Adds callback to be invoked when new keyboard layout is detected.
  void AddKeyboardModifiersCallback(KeyboardModifiersCallback callback);

  // Invoked by the wayland keyboard, upon detecting a keyboard modifier
  // changes from the compositor.
  void OnKeyboardModifiers(uint32_t group);

  // Gets the current information about displays available on the host.
  DesktopDisplayInfo GetCurrentDisplayInfo();

  void SetSeatPresentCallback(WaylandSeat::OnSeatPresentCallback callback);

  // Sets callback to be invoked when the associated seat gains a keyboard or
  // pointer capability.
  void SetCapabilityCallbacks(base::OnceClosure keyboard_capability_callback,
                              base::OnceClosure pointer_capability_callback);

 private:
  friend class WaylandSeat;

  // Invoked by wayland seat when wayland keyboard capability changes.
  void OnSeatKeyboardCapability();
  void OnSeatKeyboardCapabilityRevoked();

  // Invoked by wayland seat when wayland pointer capability changes.
  void OnSeatPointerCapability();
  void OnSeatPointerCapabilityRevoked();

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  std::unique_ptr<WaylandConnection> wayland_connection_;
  DesktopMetadataCallback capturer_metadata_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  UpdateScreenResolutionCallback screen_resolution_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  KeyboardLayoutCallback keyboard_layout_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::RepeatingCallbackList<KeyboardModifiersCallbackSignature>
      keyboard_modifier_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
  ClipboardMetadataCallback clipboard_metadata_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceClosure keyboard_capability_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceClosure pointer_capability_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OnceClosure capturer_destroyed_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Keeps track of the latest keymap for the case where the keyboard layout
  // monitor has not yet registered a callback.
  XkbKeyMapUniquePtr keymap_ GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  bool is_keyboard_capability_acquired_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  bool is_pointer_capability_acquired_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_WAYLAND_MANAGER_H_
