// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_session.h"

namespace remoting {

std::unique_ptr<TerminalSession> TerminalSession::Create(
    TerminalSessionManager::OutputCallback output_cb,
    TerminalSessionManager::ExitCallback exit_cb,
    int32_t id) {
  return nullptr;
}

// static
std::vector<int32_t> TerminalSession::GetPersistentTerminalIds() {
  return {};
}

}  // namespace remoting
