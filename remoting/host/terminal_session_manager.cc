// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_session_manager.h"

#include <utility>

#include "base/logging.h"
#include "remoting/host/terminal_session.h"

namespace remoting {

TerminalSessionManager::TerminalSessionManager() = default;
TerminalSessionManager::~TerminalSessionManager() = default;

int32_t TerminalSessionManager::CreateTerminal(OutputCallback output_callback,
                                               ExitCallback exit_callback) {
  int32_t id = next_terminal_id_++;
  std::unique_ptr<TerminalSession> session = TerminalSession::Create(
      std::move(output_callback), std::move(exit_callback), id);
  if (session == nullptr || !session->Start()) {
    return -1;
  }
  terminal_sessions_[id] = std::move(session);
  return id;
}

void TerminalSessionManager::WriteTerminal(const int32_t terminal_id,
                                           const std::string& data) {
  auto it = terminal_sessions_.find(terminal_id);
  if (it != terminal_sessions_.end()) {
    it->second->Write(data);
  } else {
    LOG(ERROR) << "Failed to find session for terminal ID: " << terminal_id;
  }
}

void TerminalSessionManager::ResizeTerminal(const int32_t terminal_id,
                                            uint32_t width,
                                            uint32_t height) {
  auto it = terminal_sessions_.find(terminal_id);
  if (it != terminal_sessions_.end()) {
    it->second->Resize(width, height);
  } else {
    LOG(ERROR) << "Failed to find session for terminal ID: " << terminal_id;
  }
}

void TerminalSessionManager::CloseTerminal(const int32_t terminal_id) {
  auto it = terminal_sessions_.find(terminal_id);
  if (it != terminal_sessions_.end()) {
    it->second->Terminate();
    terminal_sessions_.erase(it);
  } else {
    LOG(ERROR) << "Failed to find session for terminal ID: " << terminal_id;
  }
}

void TerminalSessionManager::OnClientDisconnected(const int32_t terminal_id) {
  // Just close the terminal session.
  CloseTerminal(terminal_id);
}

TerminalSession* TerminalSessionManager::GetTerminalSession(
    const int32_t terminal_id) {
  auto it = terminal_sessions_.find(terminal_id);
  if (it != terminal_sessions_.end()) {
    return it->second.get();
  }
  LOG(ERROR) << "Failed to find session for terminal ID: " << terminal_id;
  return nullptr;
}

std::vector<int32_t> TerminalSessionManager::GetTerminalSessionIds() {
  std::vector<int32_t> ids;
  for (const auto& [id, session] : terminal_sessions_) {
    ids.push_back(id);
  }
  return ids;
}

}  // namespace remoting
