// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_TERMINAL_SESSION_H_
#define REMOTING_HOST_TERMINAL_SESSION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "remoting/host/terminal_error.h"
#include "remoting/host/terminal_session_manager.h"

namespace remoting {

class TerminalSession {
 public:
  using StartCallback =
      base::OnceCallback<void(base::expected<void, TerminalError>)>;

  virtual ~TerminalSession() = default;

  // Returns true if terminal sessions are supported on the current platform.
  static bool IsSupported();

  // Factory helper to construct an OS-specific TerminalSession.
  // Returns nullptr on unsupported platforms.
  static std::unique_ptr<TerminalSession> Create(
      TerminalSessionManager::OutputCallback output_cb,
      TerminalSessionManager::ExitCallback exit_cb,
      TerminalSessionManager::ProcessInfoCallback process_info_cb,
      int32_t id);

  // Returns the IDs of all currently persistent terminal sessions.
  // Must be called on a thread that allows blocking.
  static std::vector<int32_t> GetPersistentTerminalIds();

  // Starts the terminal session. `callback` is run once the session has
  // started, or with an error describing why it could not be started. It may
  // be run synchronously. If the session fails to start, the exit callback is
  // not run. On success, `callback` is run before any output is delivered. If
  // the session is destroyed, detached or terminated before it has started,
  // `callback` is not run.
  virtual void Start(StartCallback callback) = 0;

  // Write terminal input.
  virtual void Write(const std::string& data) = 0;

  // Resize window size (rows and columns).
  virtual void Resize(uint32_t width, uint32_t height) = 0;

  // Terminate matching subprocess and destroy descriptors.
  virtual void Terminate() = 0;

  // Detaches from the terminal session without destroying the underlying
  // persistent session.
  virtual void Detach() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_TERMINAL_SESSION_H_
