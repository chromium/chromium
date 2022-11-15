// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_CLIPBOARD_WAYLAND_H_
#define REMOTING_HOST_LINUX_CLIPBOARD_WAYLAND_H_

#include "remoting/host/clipboard.h"

#include <memory>

#include "remoting/host/linux/clipboard_portal_injector.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {

class ClipboardWayland : public Clipboard {
 public:
  explicit ClipboardWayland();

  ClipboardWayland(const ClipboardWayland&) = delete;
  ClipboardWayland& operator=(const ClipboardWayland&) = delete;

  ~ClipboardWayland() override;

  // Clipboard interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  void SetSessionDetails(
      const webrtc::xdg_portal::SessionDetails& session_details);

 private:
  void OnClipboardChanged(const std::string& mime_type,
                          const std::string& data);

  std::unique_ptr<protocol::ClipboardStub> client_clipboard_;
  xdg_portal::ClipboardPortalInjector clipboard_portal_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_CLIPBOARD_WAYLAND_H_
