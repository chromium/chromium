// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_
#define REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_

#include <string_view>

#include "remoting/host/input_injector.h"

namespace remoting {

class EiSenderSession;

class GnomeInputInjector : public InputInjector {
 public:
  // The stream's mapping-id is needed for injecting absolute mouse motion.
  // Currently, there is only 1 capture-stream and its mapping-id never
  // changes during the connection lifetime.
  // TODO: crbug.com/432217140 - when multiple displays are supported, this
  // parameter should be replaced with some kind of stream-mapping. This should
  // convert the stream-id from the mouse-event's FractionalCoordinate to a
  // mapping-id. Alternatively, EiSenderSession could maintain this mapping
  // information, but this may depend on exactly how the stream-id will be
  // implemented.
  GnomeInputInjector(std::unique_ptr<EiSenderSession> session,
                     std::string_view stream_mapping_id);
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
  std::string stream_mapping_id_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_
