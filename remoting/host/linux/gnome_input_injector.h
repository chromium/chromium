// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_
#define REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_

#include <set>

#include "base/memory/weak_ptr.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/linux/clipboard_gnome.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/pipewire_capture_stream_manager.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

class EiSenderSession;
class EiKeymap;

class GnomeInputInjector : public InputInjector {
 public:
  // The stream's mapping-id is needed for injecting absolute mouse motion.
  // Currently, there is only 1 capture-stream and its mapping-id never
  // changes during the connection lifetime.
  GnomeInputInjector(
      base::WeakPtr<EiSenderSession> session,
      base::WeakPtr<const PipewireCaptureStreamManager> stream_manager,
      GDBusConnectionRef dbus_connection,
      gvariant::ObjectPath session_path);
  ~GnomeInputInjector() override;

  base::WeakPtr<GnomeInputInjector> GetWeakPtr();

  void SetKeymap(base::WeakPtr<EiKeymap> keymap);

  // InputInjector implementation
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

  // InputStub implementation
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;

  // ClipboardStub implementation
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

 private:
  base::WeakPtr<EiSenderSession> ei_session_;
  base::WeakPtr<EiKeymap> keymap_;
  base::WeakPtr<const PipewireCaptureStreamManager> stream_manager_;
  ClipboardGnome clipboard_;
  std::set<uint32_t> pressed_keys_;

  base::WeakPtrFactory<GnomeInputInjector> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_
