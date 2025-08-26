// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_EI_KEYMAP_H_
#define REMOTING_HOST_LINUX_EI_KEYMAP_H_

#include <xkbcommon/xkbcommon.h>

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "remoting/host/base/pointer_utils.h"
#include "remoting/host/linux/fd_string_reader.h"
#include "remoting/proto/control.pb.h"
#include "third_party/libei/cipd/include/libei-1.0/libei.h"
#include "ui/events/keycodes/scoped_xkb.h"

namespace remoting {

// A keymap for a given keyboard ei_device. Per the documentation, this
// is constant for the lifetime of the device and can be cached.
class EiKeymap {
 public:
  using EiDevicePtr = CRefCounted<ei_device, ei_device_ref, ei_device_unref>;

  explicit EiKeymap(EiDevicePtr keyboard);
  EiKeymap(const EiKeymap&) = delete;
  EiKeymap& operator=(const EiKeymap&) = delete;
  ~EiKeymap();

  // Load the keymap from the specified device asynchronously. Errors are
  // logged internally; the callback should check IsValid to see if the load
  // succeeded.
  void Load(base::OnceClosure callback);

  bool IsValid() const;
  xkb_keymap* Get();

  const protocol::KeyboardLayout& GetLayoutProto() const;

 private:
  void OnKeymapLoaded(base::OnceClosure callback,
                      base::expected<std::string, Loggable> result);
  static void ProcessKey(xkb_keymap* keymap,
                         xkb_keycode_t keycode,
                         void* data);

  EiDevicePtr keyboard_;
  std::unique_ptr<FdStringReader> reader_;
  std::unique_ptr<xkb_keymap, ui::XkbKeymapDeleter> keymap_;
  std::unique_ptr<xkb_state, ui::XkbStateDeleter> xkb_state_;
  protocol::KeyboardLayout layout_proto_;
  std::map<int, int> shift_level_to_mask_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_EI_KEYMAP_H_
