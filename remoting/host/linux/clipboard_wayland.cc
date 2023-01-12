// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/clipboard_wayland.h"

#include <memory>

#include "remoting/host/linux/clipboard_portal_injector.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {

ClipboardWayland::ClipboardWayland()
    : clipboard_portal_(xdg_portal::ClipboardPortalInjector(
          base::BindRepeating(&ClipboardWayland::OnClipboardChanged,
                              base::Unretained(this)))) {}

ClipboardWayland::~ClipboardWayland() {}

void ClipboardWayland::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  client_clipboard_.swap(client_clipboard);
}

void ClipboardWayland::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  clipboard_portal_.SetSelection(event.mime_type(), event.data());
}

void ClipboardWayland::SetSessionDetails(
    const webrtc::xdg_portal::SessionDetails& session_details) {
  clipboard_portal_.SetSessionDetails(session_details);
}

void ClipboardWayland::OnClipboardChanged(const std::string& mime_type,
                                          const std::string& data) {
  if (!client_clipboard_) {
    return;
  }

  protocol::ClipboardEvent event;
  event.set_mime_type(mime_type);
  event.set_data(data);

  client_clipboard_->InjectClipboardEvent(event);
}

}  // namespace remoting
