// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_INPUT_INJECTOR_H_
#define REMOTING_HOST_IPC_INPUT_INJECTOR_H_

#include "base/memory/scoped_refptr.h"
#include "remoting/host/input_injector.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

class DesktopSessionProxy;

// Routes InputInjector calls though the IPC channel to the desktop session
// agent running in the desktop integration process.
class IpcInputInjector : public InputInjector {
 public:
  explicit IpcInputInjector(
      scoped_refptr<DesktopSessionProxy> desktop_session_proxy);

  IpcInputInjector(const IpcInputInjector&) = delete;
  IpcInputInjector& operator=(const IpcInputInjector&) = delete;

  ~IpcInputInjector() override;

  // ClipboardStub interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  // InputStub interface.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;

  // InputInjector interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

 private:
  // Wraps the IPC channel to the desktop process.
  scoped_refptr<DesktopSessionProxy> desktop_session_proxy_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_INPUT_INJECTOR_H_
