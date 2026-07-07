// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TERMINAL_SESSION_H_
#define REMOTING_HOST_TERMINAL_SESSION_H_

#include <memory>
#include <string>

#include "remoting/host/terminal_session_manager.h"

namespace remoting {

class TerminalSession {
 public:
  virtual ~TerminalSession() = default;

  // Factory helper to construct an OS-specific TerminalSession.
  // Returns nullptr on unsupported platforms.
  static std::unique_ptr<TerminalSession> Create(
      TerminalSessionManager::OutputCallback output_cb,
      TerminalSessionManager::ExitCallback exit_cb,
      int32_t id);

  virtual bool Start() = 0;

  // Write terminal input.
  virtual void Write(const std::string& data) = 0;

  // Resize window size (rows and columns).
  virtual void Resize(uint32_t width, uint32_t height) = 0;

  // Terminate matching subprocess and destroy descriptors.
  virtual void Terminate() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_TERMINAL_SESSION_H_
