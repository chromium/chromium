// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FAKE_TERMINAL_SESSION_H_
#define REMOTING_HOST_FAKE_TERMINAL_SESSION_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "remoting/host/terminal_error.h"
#include "remoting/host/terminal_session.h"

namespace remoting {

// A fake implementation of TerminalSession for unit testing to mock the
// behavior of TerminalSession.
class FakeTerminalSession : public TerminalSession {
 public:
  static std::vector<base::WeakPtr<FakeTerminalSession>> GetActiveSessions();
  static bool WasTerminated(int32_t id);
  static void ResetTerminatedIds();
  static void ResetStaticState();

  // Causes the next call to Start() to fail with `error`. Pass std::nullopt to
  // clear a previously set error.
  static void SetNextStartError(std::optional<TerminalError> error);
  // If `defer` is true, subsequent calls to Start() that are not failed by
  // SetNextStartError() will not complete until CompleteStart() is called.
  static void SetDeferStart(bool defer);
  static void SetPersistentTerminalIds(std::vector<int32_t> ids);
  static std::vector<int32_t> GetPersistentIds();

  FakeTerminalSession(
      TerminalSessionManager::OutputCallback output_cb,
      TerminalSessionManager::ExitCallback exit_cb,
      TerminalSessionManager::ProcessInfoCallback process_info_cb,
      int32_t id);
  ~FakeTerminalSession() override;

  // TerminalSession implementation:
  void Start(StartCallback callback) override;
  void Write(const std::string& data) override;
  void Resize(uint32_t width, uint32_t height) override;
  void Terminate() override;
  void Detach() override;

  // Completes a deferred Start() with `result`. As with a real session, the
  // start callback is not run if the session has been detached or terminated.
  // Note that the owner may delete `this` in response.
  void CompleteStart(base::expected<void, TerminalError> result);

  int32_t id() const { return id_; }
  const std::vector<std::string>& inputs() const { return inputs_; }
  const std::vector<std::pair<uint32_t, uint32_t>>& resizes() const {
    return resizes_;
  }
  bool is_started() const { return is_started_; }
  bool is_terminated() const { return is_terminated_; }
  bool is_detached() const { return is_detached_; }
  bool has_pending_start() const { return !pending_start_callback_.is_null(); }

  void TriggerOutput(const std::string& data);
  void TriggerExit();
  void TriggerProcessInfo(bool is_active,
                          std::string_view process_name = "test-process");

 private:
  TerminalSessionManager::OutputCallback output_cb_;
  TerminalSessionManager::ExitCallback exit_cb_;
  TerminalSessionManager::ProcessInfoCallback process_info_cb_;
  int32_t id_;
  StartCallback pending_start_callback_;
  std::vector<std::string> inputs_;
  std::vector<std::pair<uint32_t, uint32_t>> resizes_;
  bool is_started_ = false;
  bool is_terminated_ = false;
  bool is_detached_ = false;

  base::WeakPtrFactory<FakeTerminalSession> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_FAKE_TERMINAL_SESSION_H_
