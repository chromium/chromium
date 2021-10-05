// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_devtools_agent.h"
#include "content/services/auction_worklet/auction_v8_devtools_session.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/sequenced_task_runner.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/debug_command_queue.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace auction_worklet {

AuctionV8DevToolsAgent::ContextGroupInfo::ContextGroupInfo() = default;
AuctionV8DevToolsAgent::ContextGroupInfo::~ContextGroupInfo() = default;

AuctionV8DevToolsAgent::AuctionV8DevToolsAgent(
    AuctionV8Helper* v8_helper,
    scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence)
    : v8_helper_(v8_helper),
      io_session_receiver_sequence_(std::move(io_session_receiver_sequence)) {}

AuctionV8DevToolsAgent::~AuctionV8DevToolsAgent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // DestroySessions must be called before the destructor.
  DCHECK(sessions_.empty());
}

void AuctionV8DevToolsAgent::Connect(
    mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent,
    int context_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  receivers_.Add(this, std::move(agent), context_group_id);
}

void AuctionV8DevToolsAgent::DestroySessions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  sessions_.Clear();
}

void AuctionV8DevToolsAgent::AttachDevToolsSession(
    mojo::PendingAssociatedRemote<blink::mojom::DevToolsSessionHost> host,
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsSession>
        session_receiver,
    mojo::PendingReceiver<blink::mojom::DevToolsSession> io_session_receiver,
    blink::mojom::DevToolsSessionStatePtr reattach_session_state,
    bool client_expects_binary_responses,
    const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  int context_group_id = receivers_.current_context();
  ContextGroupInfo& context_group_info = context_groups_[context_group_id];

  // Make the actual session and bind it to the pipe.
  AuctionV8DevToolsSession::SessionDestroyedCallback session_destroyed =
      base::BindOnce(
          &AuctionV8DevToolsAgent::SessionDestroyed,
          // `sessions_` guarantees `session_impl` won't outlast `this`.
          base::Unretained(this));
  auto session_impl = std::make_unique<AuctionV8DevToolsSession>(
      v8_helper_, context_group_info.command_queue, context_group_id,
      session_id, client_expects_binary_responses, std::move(host),
      io_session_receiver_sequence_, std::move(io_session_receiver),
      std::move(session_destroyed));
  sessions_.Add(std::move(session_impl), std::move(session_receiver));

  // Keep track of sessions for given worklet, to help cleanup its
  // DebugCommandQueue.
  context_group_info.sessions.insert(session_impl.get());
}

void AuctionV8DevToolsAgent::InspectElement(const ::gfx::Point& point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();  // Should not be used with this.
}

void AuctionV8DevToolsAgent::ReportChildWorkers(
    bool report,
    bool wait_for_debugger,
    ReportChildWorkersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();  // Should not be used with this.
}

void AuctionV8DevToolsAgent::runMessageLoopOnPause(int context_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  DCHECK(!paused_context_group_queue_);

  auto it = context_groups_.find(context_group_id);
  DCHECK(it != context_groups_.end());

  paused_context_group_queue_ = it->second.command_queue;
  v8_helper_->PauseTimeoutTimer();
  paused_context_group_queue_->PauseForDebuggerAndRunCommands();
  v8_helper_->ResumeTimeoutTimer();
  paused_context_group_queue_ = nullptr;
}

void AuctionV8DevToolsAgent::quitMessageLoopOnPause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  DCHECK(paused_context_group_queue_);
  paused_context_group_queue_->QuitPauseForDebugger();
}

void AuctionV8DevToolsAgent::runIfWaitingForDebugger(int context_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_helper_->Resume(context_group_id);
}

void AuctionV8DevToolsAgent::SessionDestroyed(
    AuctionV8DevToolsSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  auto it = context_groups_.find(session->context_group_id());
  DCHECK(it != context_groups_.end());
  it->second.sessions.erase(session);
  if (it->second.sessions.empty()) {
    // TODO(morlovich): This currently can't happen since we can't see
    // SessionDestroyed when paused, but the scenario of navigating away when
    // paused may be a trouble spot.
    DCHECK_NE(it->second.command_queue, paused_context_group_queue_);
    context_groups_.erase(it);
  }
}

}  // namespace auction_worklet
