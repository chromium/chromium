// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/clipboard_portal.h"

#include <memory>

#include "base/notimplemented.h"
#include "remoting/protocol/clipboard_stub.h"

namespace remoting {

ClipboardPortal::ClipboardPortal() = default;
ClipboardPortal::~ClipboardPortal() = default;

void ClipboardPortal::Start(
    std::unique_ptr<protocol::ClipboardStub> client_clipboard) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ClipboardPortal::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace remoting
