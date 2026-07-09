// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TERMINAL_SESSION_MANAGER_H_
#define REMOTING_HOST_TERMINAL_SESSION_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace remoting {

// Manages the lifecycles of active TerminalSession instances.
class TerminalSession;

class TerminalSessionManager {
 public:
  // Callback invoked when the terminal outputs data. The callback is called
  // with the terminal ID and the output data.
  using OutputCallback =
      base::RepeatingCallback<void(int32_t, const std::string&)>;

  using ExitCallback = base::OnceCallback<void(int32_t)>;

  TerminalSessionManager();
  ~TerminalSessionManager();

  TerminalSessionManager(const TerminalSessionManager&) = delete;
  TerminalSessionManager& operator=(const TerminalSessionManager&) = delete;

  // Spawns a new terminal session. Returns the ID of the new session, or -1 if
  // the terminal could not be created.
  int32_t CreateTerminal(OutputCallback output_callback,
                         ExitCallback exit_callback);

  // Writes data to the terminal session.
  void WriteTerminal(const int32_t terminal_id, const std::string& data);

  // Resizes the terminal rows and columns for the given terminal session.
  void ResizeTerminal(const int32_t terminal_id,
                      uint32_t width,
                      uint32_t height);

  // Closes and destroys the terminal session.
  void CloseTerminal(const int32_t terminal_id);

  // Called when a client session is disconnected.
  void OnClientDisconnected();

  // Returns the terminal session with the given ID.
  TerminalSession* GetTerminalSession(const int32_t terminal_id);

  // Returns the terminal session IDs.
  std::vector<int32_t> GetTerminalSessionIds();

 private:
  // Intercepts the exit callback from the TerminalSession.
  void OnTerminalExited(ExitCallback client_callback, int32_t terminal_id);

  int32_t next_terminal_id_ = 1;
  std::map<int32_t, std::unique_ptr<TerminalSession>> terminal_sessions_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_TERMINAL_SESSION_MANAGER_H_
