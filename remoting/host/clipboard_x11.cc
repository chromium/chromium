// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/clipboard.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "remoting/host/linux/x_server_clipboard.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_types.h"

namespace remoting {

// This code is expected to be called on the desktop thread only.
class ClipboardX11 : public Clipboard, public x11::EventObserver {
 public:
  ClipboardX11();

  ClipboardX11(const ClipboardX11&) = delete;
  ClipboardX11& operator=(const ClipboardX11&) = delete;

  ~ClipboardX11() override;

  void Init();

  // Clipboard interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

 private:
  void OnClipboardChanged(const std::string& mime_type,
                          const std::string& data);

  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

  std::unique_ptr<protocol::ClipboardStub> client_clipboard_;

  // Underlying X11 clipboard implementation.
  XServerClipboard x_server_clipboard_;

  // Connection to the X server, used by |x_server_clipboard_|. This must only
  // be accessed on the input thread.
  raw_ptr<x11::Connection> connection_;
};

ClipboardX11::ClipboardX11() = default;

ClipboardX11::~ClipboardX11() {
  if (connection_) {
    connection_->RemoveEventObserver(this);
  }
}

void ClipboardX11::Init() {
  connection_ = x11::Connection::Get();
  connection_->AddEventObserver(this);
  x_server_clipboard_.Init(
      connection_, base::BindRepeating(&ClipboardX11::OnClipboardChanged,
                                       base::Unretained(this)));
}

void ClipboardX11::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  client_clipboard_.swap(client_clipboard);
}

void ClipboardX11::InjectClipboardEvent(const protocol::ClipboardEvent& event) {
  x_server_clipboard_.SetClipboard(event.mime_type(), event.data());
}

void ClipboardX11::OnClipboardChanged(const std::string& mime_type,
                                      const std::string& data) {
  protocol::ClipboardEvent event;
  event.set_mime_type(mime_type);
  event.set_data(data);

  if (client_clipboard_.get()) {
    client_clipboard_->InjectClipboardEvent(event);
  }
}

void ClipboardX11::OnEvent(const x11::Event& event) {
  x_server_clipboard_.ProcessXEvent(event);
}

std::unique_ptr<Clipboard> Clipboard::Create() {
  auto clipboard = std::make_unique<ClipboardX11>();
  clipboard->Init();
  return clipboard;
}

}  // namespace remoting
