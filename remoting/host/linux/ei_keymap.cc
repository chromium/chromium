// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/ei_keymap.h"

#include <string_view>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "remoting/proto/control.pb.h"

namespace remoting {

EiKeymap::EiKeymap(EiDevicePtr keyboard) : keyboard_(keyboard) {}

EiKeymap::~EiKeymap() = default;

void EiKeymap::Load(base::OnceClosure callback) {
  base::ScopedClosureRunner closure_runner(std::move(callback));
  // Get the keymap file descriptor and verify that it's (at time of writing)
  // the only supported type.
  struct ei_keymap* keymap = ei_device_keyboard_get_keymap(keyboard_.get());
  if (!keymap) {
    LOG(ERROR) << "No keymap found for the current keyboard";
    return;
  }
  auto type = ei_keymap_get_type(keymap);
  if (type != EI_KEYMAP_TYPE_XKB) {
    LOG(ERROR) << "Unsupported keymap type: " << type;
    return;
  }
  // The file descriptor is owned by libei so needs to be dup'd before using
  // ScopedFD.
  auto original_fd = ei_keymap_get_fd(keymap);
  base::ScopedFD fd(HANDLE_EINTR(dup(original_fd)));
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to duplicate keymap file descriptor";
    return;
  }
  // Since the reader as a member variable, base::Unretained is safe.
  reader_ = FdStringReader::ReadFromFile(
      std::move(fd),
      base::BindOnce(&EiKeymap::OnKeymapLoaded, base::Unretained(this),
                     closure_runner.Release()));
}

bool EiKeymap::IsValid() const {
  return keymap_ != nullptr;
}

xkb_keymap* EiKeymap::Get() {
  return keymap_.get();
}

void EiKeymap::OnKeymapLoaded(base::OnceClosure callback,
                              base::expected<std::string, Loggable> result) {
  base::ScopedClosureRunner closure_runner(std::move(callback));
  if (!result.has_value()) {
    LOG(ERROR) << "Reading keymap failed: " << result.error();
    return;
  }
  // Create an xkb_keymap object from the mapped string.
  std::unique_ptr<xkb_context, ui::XkbContextDeleter> ctx{
      xkb_context_new(XKB_CONTEXT_NO_FLAGS)};
  if (!ctx) {
    LOG(ERROR) << "Failed to create XKB context";
    return;
  }
  keymap_.reset(xkb_keymap_new_from_string(ctx.get(), result.value().c_str(),
                                           XKB_KEYMAP_FORMAT_TEXT_V1,
                                           XKB_KEYMAP_COMPILE_NO_FLAGS));
}

}  // namespace remoting
