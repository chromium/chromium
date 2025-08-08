// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_
#define REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_

#include "base/memory/weak_ptr.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/linux/pipewire_capture_stream_manager.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

class EiSenderSession;

class GnomeInputInjector : public InputInjector {
 public:
  // The stream's mapping-id is needed for injecting absolute mouse motion.
  // Currently, there is only 1 capture-stream and its mapping-id never
  // changes during the connection lifetime.
  GnomeInputInjector(
      std::unique_ptr<EiSenderSession> session,
      base::WeakPtr<const PipewireCaptureStreamManager> stream_manager);
  ~GnomeInputInjector() override;

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
  std::unique_ptr<EiSenderSession> ei_session_;
  base::WeakPtr<const PipewireCaptureStreamManager> stream_manager_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_
