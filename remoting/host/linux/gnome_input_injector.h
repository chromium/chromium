// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_
#define REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_

#include "base/memory/weak_ptr.h"
#include "remoting/host/input_injector.h"

namespace remoting {

class GnomeInteractionStrategy;

class GnomeInputInjector : public InputInjector {
 public:
  explicit GnomeInputInjector(base::WeakPtr<GnomeInteractionStrategy> session);
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
  base::WeakPtr<GnomeInteractionStrategy> session_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_INPUT_INJECTOR_H_
