// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_v8_devtools_agent.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "content/services/auction_worklet/auction_v8_devtools_session.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/debug_command_queue.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace auction_worklet {

AuctionV8DevToolsAgent::ContextGroupInfo::ContextGroupInfo() = default;
AuctionV8DevToolsAgent::ContextGroupInfo::~ContextGroupInfo() = default;

AuctionV8DevToolsAgent::AuctionV8DevToolsAgent(
    AuctionV8Helper* v8_helper,
    scoped_refptr<DebugCommandQueue> debug_command_queue,
    scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence)
    : v8_helper_(v8_helper),
      io_session_receiver_sequence_(std::move(io_session_receiver_sequence)),
      debug_command_queue_(debug_command_queue.get()) {}

AuctionV8DevToolsAgent::~AuctionV8DevToolsAgent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // DestroySessions must be called before the destructor.
  DCHECK(sessions_.empty());
}

void AuctionV8DevToolsAgent::Connect(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
    int context_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  receivers_.Add(this, std::move(agent), context_group_id);
}

void AuctionV8DevToolsAgent::MaybeTriggerInstrumentationBreakpoint(
    int context_group_id,
    const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  auto it = context_groups_.find(context_group_id);
  if (it == context_groups_.end())
    return;  // No sessions, so no breakpoints.

  for (AuctionV8DevToolsSession* session : it->second.sessions) {
    session->MaybeTriggerInstrumentationBreakpoint(name);
  }
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
    bool client_is_trusted,
    const std::string& session_id,
    bool session_waits_for_debugger) {
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
      v8_helper_, scoped_refptr<DebugCommandQueue>(debug_command_queue_),
      context_group_id, session_id, client_expects_binary_responses,
      session_waits_for_debugger, std::move(host),
      io_session_receiver_sequence_, std::move(io_session_receiver),
      std::move(session_destroyed));
  context_group_info.sessions.insert(session_impl.get());
  sessions_.Add(std::move(session_impl), std::move(session_receiver));
}

void AuctionV8DevToolsAgent::InspectElement(const ::gfx::Point& point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();  // Should not be used with this.
}

void AuctionV8DevToolsAgent::ReportChildTargets(
    bool report,
    bool wait_for_debugger,
    ReportChildTargetsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();  // Should not be used with this.
}

void AuctionV8DevToolsAgent::runMessageLoopOnPause(int context_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  DCHECK(!paused_);

  auto it = context_groups_.find(context_group_id);
  CHECK(it != context_groups_.end(), base::NotFatalUntil::M130);
  DCHECK(!it->second.sessions.empty());
  AuctionV8DevToolsSession* session = *it->second.sessions.begin();

  v8_helper_->PauseTimeoutTimer();
  paused_ = true;
  base::OnceClosure abort_callback = session->MakeAbortPauseCallback();
  debug_command_queue_->PauseForDebuggerAndRunCommands(
      context_group_id, std::move(abort_callback));
  DCHECK(paused_);
  v8_helper_->ResumeTimeoutTimer();
  paused_ = false;
}

void AuctionV8DevToolsAgent::quitMessageLoopOnPause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  DCHECK(paused_);
  debug_command_queue_->QuitPauseForDebugger();
}

void AuctionV8DevToolsAgent::runIfWaitingForDebugger(int context_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_helper_->Resume(context_group_id);
}

void AuctionV8DevToolsAgent::SessionDestroyed(
    AuctionV8DevToolsSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  auto it = context_groups_.find(session->context_group_id());
  CHECK(it != context_groups_.end(), base::NotFatalUntil::M130);
  it->second.sessions.erase(session);
  if (it->second.sessions.empty())
    context_groups_.erase(it);
}

}  // namespace auction_worklet
