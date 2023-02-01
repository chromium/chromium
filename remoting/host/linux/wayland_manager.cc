// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_manager.h"
#include "remoting/base/logging.h"

#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

WaylandManager::WaylandManager() {}

WaylandManager::~WaylandManager() {}

// static
WaylandManager* WaylandManager::Get() {
  static base::NoDestructor<WaylandManager> instance;
  return instance.get();
}

void WaylandManager::Init(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
  const char* wayland_display = getenv("WAYLAND_DISPLAY");
  if (!wayland_display) {
    LOG(WARNING) << "WAYLAND_DISPLAY env variable is not set";
    return;
  }
  wayland_connection_ =
      std::make_unique<WaylandConnection>(getenv("WAYLAND_DISPLAY"));
}

void WaylandManager::CleanupRunnerForTest() {
  ui_task_runner_ = nullptr;
}

void WaylandManager::AddCapturerMetadataCallback(
    DesktopMetadataCallback callback) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::AddCapturerMetadataCallback,
                       base::Unretained(this),
                       base::BindPostTask(
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(callback))));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  capturer_metadata_callback_ = std::move(callback);
}

void WaylandManager::AddCapturerDestroyedCallback(base::OnceClosure callback) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::AddCapturerDestroyedCallback,
                       base::Unretained(this),
                       base::BindPostTask(
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(callback))));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  capturer_destroyed_callback_ = std::move(callback);
}

void WaylandManager::OnDesktopCapturerMetadata(
    webrtc::DesktopCaptureMetadata metadata) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::OnDesktopCapturerMetadata,
                                  base::Unretained(this), std::move(metadata)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (capturer_metadata_callback_) {
    capturer_metadata_callback_.Run(std::move(metadata));
  } else {
    LOG(ERROR) << "Expected the capturer metadata observer to have register "
               << "a callback by now";
  }
}

void WaylandManager::OnDesktopCapturerDestroyed() {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::OnDesktopCapturerDestroyed,
                                  base::Unretained(this)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (capturer_destroyed_callback_) {
    std::move(capturer_destroyed_callback_).Run();
  } else {
    LOG(ERROR) << "Expected the capturer destruction observer to have register "
               << "a callback by now";
  }
}

void WaylandManager::AddClipboardMetadataCallback(
    DesktopMetadataCallback callback) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::AddClipboardMetadataCallback,
                       base::Unretained(this),
                       base::BindPostTask(
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(callback))));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  clipboard_metadata_callback_ = std::move(callback);
}

void WaylandManager::OnClipboardMetadata(
    webrtc::DesktopCaptureMetadata metadata) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::OnClipboardMetadata,
                                  base::Unretained(this), std::move(metadata)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (clipboard_metadata_callback_) {
    clipboard_metadata_callback_.Run(std::move(metadata));
  } else {
    LOG(WARNING) << "Expected the clipboard observer to have registered "
                 << "a callback by now";
  }
}

void WaylandManager::AddUpdateScreenResolutionCallback(
    UpdateScreenResolutionCallback callback) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::AddUpdateScreenResolutionCallback,
                       base::Unretained(this),
                       base::BindPostTask(
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(callback))));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  screen_resolution_callback_ = std::move(callback);
}

void WaylandManager::OnUpdateScreenResolution(ScreenResolution resolution,
                                              webrtc::ScreenId screen_id) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::OnUpdateScreenResolution,
                                  base::Unretained(this), std::move(resolution),
                                  screen_id));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (screen_resolution_callback_) {
    screen_resolution_callback_.Run(std::move(resolution), screen_id);
  } else {
    LOG(WARNING) << "Expected the screen resolution observer to have register "
                 << "a callback by now";
  }
}

void WaylandManager::SetSeatPresentCallback(
    WaylandSeat::OnSeatPresentCallback callback) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::SetSeatPresentCallback,
                                  base::Unretained(this), std::move(callback)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  wayland_connection_->SetSeatPresentCallback(std::move(callback));
}

void WaylandManager::SetKeyboardLayoutCallback(
    KeyboardLayoutCallback callback) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::SetKeyboardLayoutCallback,
                       base::Unretained(this),
                       base::BindPostTask(
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(callback))));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  keyboard_layout_callback_ = std::move(callback);
  if (keymap_) {
    keyboard_layout_callback_.Run(std::move(keymap_));
  }
}

void WaylandManager::OnKeyboardLayout(XkbKeyMapUniquePtr keymap) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::OnKeyboardLayout,
                                  base::Unretained(this), std::move(keymap)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (keyboard_layout_callback_) {
    keyboard_layout_callback_.Run(std::move(keymap));
  } else {
    keymap_ = std::move(keymap);
  }
}

void WaylandManager::AddKeyboardModifiersCallback(
    KeyboardModifiersCallback callback) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::AddKeyboardModifiersCallback,
                       base::Unretained(this),
                       base::BindPostTask(
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(callback))));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  keyboard_modifier_callbacks_.AddUnsafe(std::move(callback));
}

void WaylandManager::SetCapabilityCallbacks(
    base::OnceClosure keyboard_capability_callback,
    base::OnceClosure pointer_capability_callback) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::SetCapabilityCallbacks,
                       base::Unretained(this),
                       base::BindPostTask(
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(keyboard_capability_callback)),
                       base::BindPostTask(
                           base::SingleThreadTaskRunner::GetCurrentDefault(),
                           std::move(pointer_capability_callback))));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  keyboard_capability_callback_ = std::move(keyboard_capability_callback);
  pointer_capability_callback_ = std::move(pointer_capability_callback);
  if (is_keyboard_capability_acquired_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(keyboard_capability_callback_));
  }
  if (is_pointer_capability_acquired_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(pointer_capability_callback_));
  }
}

void WaylandManager::OnSeatKeyboardCapabilityRevoked() {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::OnSeatKeyboardCapabilityRevoked,
                       base::Unretained(this)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_keyboard_capability_acquired_ = false;
}

void WaylandManager::OnSeatKeyboardCapability() {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::OnSeatKeyboardCapability,
                                  base::Unretained(this)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_keyboard_capability_acquired_ = true;

  const uint32_t seat_id = wayland_connection_->GetSeatId();
  if (!keyboard_capability_callback_) {
    LOG(WARNING) << "Seat (" << seat_id << ") gained keyboard capability "
                 << "before a listener is registered.";
  } else {
    std::move(keyboard_capability_callback_).Run();
  }
}

void WaylandManager::OnSeatPointerCapabilityRevoked() {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandManager::OnSeatPointerCapabilityRevoked,
                       base::Unretained(this)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_pointer_capability_acquired_ = false;
}

void WaylandManager::OnSeatPointerCapability() {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::OnSeatPointerCapability,
                                  base::Unretained(this)));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_pointer_capability_acquired_ = true;

  const uint32_t seat_id = wayland_connection_->GetSeatId();
  if (!pointer_capability_callback_) {
    LOG(WARNING) << "Seat (" << seat_id << ") gained pointer capability "
                 << "before a listener is registered.";
  } else {
    std::move(pointer_capability_callback_).Run();
  }
}

void WaylandManager::OnKeyboardModifiers(uint32_t group) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandManager::OnKeyboardModifiers,
                                  base::Unretained(this), group));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  keyboard_modifier_callbacks_.Notify(group);
}

DesktopDisplayInfo WaylandManager::GetCurrentDisplayInfo() {
  return wayland_connection_->GetCurrentDisplayInfo();
}

}  // namespace remoting
