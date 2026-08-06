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

  using ExitCallback = base::RepeatingCallback<void(int32_t)>;

  TerminalSessionManager();
  ~TerminalSessionManager();

  TerminalSessionManager(const TerminalSessionManager&) = delete;
  TerminalSessionManager& operator=(const TerminalSessionManager&) = delete;

  // Restores all persistent terminal sessions asynchronously.
  //
  // Expected Usage:
  // This will be called once in PeerSessionImpl when a connection is
  // established. Should be called before any other TerminalSessionManager
  // methods.
  //
  // Behavior:
  // - GetTerminalSessionIds() returns the list of terminal session IDs that
  //   have been restored so far.
  // - GetTerminalSession(id) returns the TerminalSession pointer for any session
  //   that has already been restored. It is safe to call with any ID returned
  //   by GetTerminalSessionIds().
  // - If CreateTerminal() is called while restoration is in progress, it will
  //   fail.
  // - If WriteTerminal() or ResizeTerminal() is called on a session that has
  //   not yet been restored, it will fail.
  void Start(OutputCallback output_callback, ExitCallback exit_callback);

  // Spawns a new terminal session using the stored output and exit callbacks.
  // Returns the ID of the new session, or -1 if the terminal could not be created.
  int32_t CreateTerminal();

  // Writes data to the terminal session.
  void WriteTerminal(const int32_t terminal_id, const std::string& data);

  // Resizes the terminal rows and columns for the given terminal session.
  void ResizeTerminal(const int32_t terminal_id,
                      uint32_t width,
                      uint32_t height);

  // Closes and destroys the terminal session.
  void CloseTerminal(const int32_t terminal_id);

  // Detaches all terminal sessions from the manager.
  void DetachAllSessions();

  // Returns the terminal session with the given ID.
  TerminalSession* GetTerminalSession(const int32_t terminal_id);

  // Returns the terminal session IDs.
  std::vector<int32_t> GetTerminalSessionIds();

 private:
  // Intercepts the exit callback from the TerminalSession.
  void OnTerminalExited(int32_t terminal_id);

  void OnPersistentTerminalIdsRetrieved(
      const std::vector<int32_t>& restored_ids);

  // Restores a terminal session with the given ID.
  void RestoreTerminal(int32_t terminal_id);

  OutputCallback output_callback_;
  ExitCallback exit_callback_;
  std::map<int32_t, std::unique_ptr<TerminalSession>> terminal_sessions_;
  int32_t next_id_ = 1;
  bool is_restoring_ = false;
  bool is_detached_ = false;
  base::WeakPtrFactory<TerminalSessionManager> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_TERMINAL_SESSION_MANAGER_H_
