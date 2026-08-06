// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_session_manager.h"

#include <limits>
#include <set>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "remoting/host/terminal_session.h"

namespace remoting {

TerminalSessionManager::TerminalSessionManager() = default;
TerminalSessionManager::~TerminalSessionManager() {
  DetachAllSessions();
}

void TerminalSessionManager::Start(OutputCallback output_callback,
                                     ExitCallback exit_callback) {
  DCHECK(terminal_sessions_.empty());
  if (is_restoring_) {
    LOG(ERROR) << "Cannot restore persistent terminals while already restoring";
    return;
  }
  output_callback_ = std::move(output_callback);
  exit_callback_ = std::move(exit_callback);
  is_restoring_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&TerminalSession::GetPersistentTerminalIds),
      base::BindOnce(&TerminalSessionManager::OnPersistentTerminalIdsRetrieved,
                     weak_factory_.GetWeakPtr()));
}

int32_t TerminalSessionManager::CreateTerminal() {
  if (is_restoring_) {
    LOG(ERROR) << "Cannot create terminal while restoring persistent terminals";
    return -1;
  }
  if (next_id_ == std::numeric_limits<int32_t>::max()) {
    LOG(ERROR) << "Maximum terminal ID reached";
    return -1;
  }
  int32_t id = next_id_++;

  auto wrapped_exit_callback =
      base::BindRepeating(&TerminalSessionManager::OnTerminalExited,
                          weak_factory_.GetWeakPtr());

  std::unique_ptr<TerminalSession> session =
      TerminalSession::Create(output_callback_, std::move(wrapped_exit_callback), id);
  if (!session) {
    return -1;
  }
  auto* session_ptr = session.get();
  terminal_sessions_[id] = std::move(session);
  base::WeakPtr<TerminalSessionManager> weak_this = weak_factory_.GetWeakPtr();
  if (!session_ptr->Start()) {
    if (weak_this) {
      weak_this->terminal_sessions_.erase(id);
    }
    return -1;
  }
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
    std::unique_ptr<TerminalSession> session = std::move(it->second);
    terminal_sessions_.erase(it);
    session->Terminate();
  } else {
    LOG(ERROR) << "Failed to find session for terminal ID: " << terminal_id;
  }
}

void TerminalSessionManager::DetachAllSessions() {
  if (is_detached_) {
    return;
  }
  is_detached_ = true;

  // Invalidate all weak pointers and doom the factory to prevent any pending or
  // future callbacks from executing after detachment.
  weak_factory_.InvalidateWeakPtrsAndDoom();
  is_restoring_ = false;
  std::map<int32_t, std::unique_ptr<TerminalSession>> sessions;
  sessions.swap(terminal_sessions_);
  for (auto& [id, session] : sessions) {
    session->Detach();
  }
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

void TerminalSessionManager::OnTerminalExited(int32_t terminal_id) {
  auto it = terminal_sessions_.find(terminal_id);
  if (it != terminal_sessions_.end()) {
    std::unique_ptr<TerminalSession> session = std::move(it->second);
    terminal_sessions_.erase(it);

    // Post a task to close the terminal session so the TerminalSession object
    // is not deleted synchronously while executing its own exit callback.
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(session));
  }

  if (exit_callback_) {
    exit_callback_.Run(terminal_id);
  }
}

void TerminalSessionManager::OnPersistentTerminalIdsRetrieved(
    const std::vector<int32_t>& restored_ids) {
  std::set<int32_t> ids_to_restore;
  for (int32_t id : restored_ids) {
    if (id <= 0) {
      LOG(WARNING) << "Ignoring invalid persistent terminal ID: " << id;
      continue;
    }
    // Update the next ID to be the next ID after the largest restored ID.
    // If the ID is the maximum value, then we will keep the next ID at the
    // maximum value.
    if (id >= next_id_) {
      next_id_ = (id == std::numeric_limits<int32_t>::max()) ? id : id + 1;
    }
    // Skip duplicate IDs in the restored IDs list.
    if (!ids_to_restore.insert(id).second) {
      LOG(INFO) << "Ignoring duplicate terminal ID: " << id;
    }
  }

  for (int32_t id : ids_to_restore) {
    RestoreTerminal(id);
  }
  is_restoring_ = false;
}

void TerminalSessionManager::RestoreTerminal(int32_t terminal_id) {
  if (terminal_id <= 0) {
    LOG(ERROR) << "Invalid terminal ID: " << terminal_id;
    return;
  }
  auto wrapped_exit_callback =
      base::BindRepeating(&TerminalSessionManager::OnTerminalExited,
                          weak_factory_.GetWeakPtr());

  std::unique_ptr<TerminalSession> session =
      TerminalSession::Create(output_callback_,
                              std::move(wrapped_exit_callback), terminal_id);
  if (!session) {
    LOG(ERROR) << "Failed to restore terminal session for ID: " << terminal_id;
    return;
  }
  auto* session_ptr = session.get();
  terminal_sessions_[terminal_id] = std::move(session);
  base::WeakPtr<TerminalSessionManager> weak_this = weak_factory_.GetWeakPtr();
  // Start the terminal session. If it fails, remove the session from the
  // terminal sessions map.
  if (!session_ptr->Start()) {
    if (weak_this) {
      weak_this->terminal_sessions_.erase(terminal_id);
    }
    LOG(ERROR) << "Failed to restore terminal session for ID: " << terminal_id;
    return;
  }
}

}  // namespace remoting
