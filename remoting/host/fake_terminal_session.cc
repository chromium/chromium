// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/fake_terminal_session.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/no_destructor.h"

namespace remoting {

namespace {
std::vector<FakeTerminalSession*>& GetActiveSessionList() {
  static base::NoDestructor<std::vector<FakeTerminalSession*>>
      active_session_list;
  return *active_session_list;
}

std::vector<int32_t>& GetTerminatedSessionIdList() {
  static base::NoDestructor<std::vector<int32_t>> terminated_session_id_list;
  return *terminated_session_id_list;
}
}  // namespace

// static
bool FakeTerminalSession::next_start_fail_ = false;

// static
std::vector<base::WeakPtr<FakeTerminalSession>>
FakeTerminalSession::GetActiveSessions() {
  std::vector<base::WeakPtr<FakeTerminalSession>> active_sessions;
  for (auto* session : GetActiveSessionList()) {
    active_sessions.push_back(session->weak_factory_.GetWeakPtr());
  }
  return active_sessions;
}

// static
bool FakeTerminalSession::WasTerminated(int32_t id) {
  const auto& terminated_session_id_list = GetTerminatedSessionIdList();
  return std::find(terminated_session_id_list.begin(),
                   terminated_session_id_list.end(),
                   id) != terminated_session_id_list.end();
}

// static
void FakeTerminalSession::ResetTerminatedIds() {
  GetTerminatedSessionIdList().clear();
}

// static
void FakeTerminalSession::ResetStaticState() {
  next_start_fail_ = false;
  ResetTerminatedIds();
}

// static
void FakeTerminalSession::SetNextStartFail(bool fail) {
  next_start_fail_ = fail;
}

// static
std::unique_ptr<TerminalSession> TerminalSession::Create(
    TerminalSessionManager::OutputCallback output_cb,
    TerminalSessionManager::ExitCallback exit_cb,
    int32_t id) {
  return std::make_unique<FakeTerminalSession>(std::move(output_cb),
                                               std::move(exit_cb), id);
}

FakeTerminalSession::FakeTerminalSession(
    TerminalSessionManager::OutputCallback output_cb,
    TerminalSessionManager::ExitCallback exit_cb,
    int32_t id)
    : output_cb_(std::move(output_cb)), exit_cb_(std::move(exit_cb)), id_(id) {
  GetActiveSessionList().push_back(this);
}

FakeTerminalSession::~FakeTerminalSession() {
  auto& active_session_list = GetActiveSessionList();
  std::erase(active_session_list, this);
}

bool FakeTerminalSession::Start() {
  if (next_start_fail_) {
    next_start_fail_ = false;
    return false;
  }
  is_started_ = true;
  return true;
}

void FakeTerminalSession::Write(const std::string& data) {
  inputs_.push_back(data);
}

void FakeTerminalSession::Resize(uint32_t width, uint32_t height) {
  resizes_.emplace_back(width, height);
}

void FakeTerminalSession::Terminate() {
  if (is_terminated_) {
    return;
  }
  is_terminated_ = true;
  GetTerminatedSessionIdList().push_back(id_);
}

void FakeTerminalSession::TriggerOutput(const std::string& data) {
  output_cb_.Run(id_, data);
}

void FakeTerminalSession::TriggerExit() {
  if (exit_cb_) {
    std::move(exit_cb_).Run(id_);
  }
}

}  // namespace remoting
