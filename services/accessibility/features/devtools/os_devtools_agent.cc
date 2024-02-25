// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/devtools/os_devtools_agent.h"

#include <memory>

#include "services/accessibility/features/devtools/debug_command_queue.h"
#include "services/accessibility/features/devtools/os_devtools_session.h"
#include "services/accessibility/features/v8_manager.h"
namespace ax {

OSDevToolsAgent::OSDevToolsAgent(
    V8Environment& v8_env,
    scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence)
    : v8_env_(v8_env),
      debug_command_queue_(base::MakeRefCounted<DebugCommandQueue>()),
      io_session_task_runner_(std::move(io_session_receiver_sequence)),
      v8_inspector_(
          v8_inspector::V8Inspector::create(v8_env.GetIsolate(), this)) {
  v8::Isolate::Scope isolate_scope(v8_env.GetIsolate());
  v8::HandleScope handle_scope(v8_env.GetIsolate());
  v8_inspector::V8ContextInfo info(v8_env.GetContext(),
                                   V8Environment::kDefaultContextId,
                                   v8_inspector::StringView());

  // Also notify that the context was created. The context already exists,
  // but the inspector was just created and also needs to be informed.
  v8_inspector_->contextCreated(info);
}

OSDevToolsAgent::~OSDevToolsAgent() {
  v8::Isolate::Scope isolate_scope(v8_env_->GetIsolate());
  v8::HandleScope handle_scope(v8_env_->GetIsolate());
  auto ctx = v8_env_->GetContext();
  v8_inspector_->contextDestroyed(ctx);
}

void OSDevToolsAgent::Connect(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  receiver_.Bind(std::move(agent));
}

std::unique_ptr<v8_inspector::V8InspectorSession>
OSDevToolsAgent::ConnectSession(OSDevToolsSession* session,
                                bool session_waits_for_debugger) {
  return v8_inspector_->connect(
      V8Environment::kDefaultContextId, session, v8_inspector::StringView(),
      v8_inspector::V8Inspector::kFullyTrusted,
      session_waits_for_debugger
          ? v8_inspector::V8Inspector::kWaitingForDebugger
          : v8_inspector::V8Inspector::kNotWaitingForDebugger);
}

void OSDevToolsAgent::MaybeTriggerInstrumentationBreakpoint(
    const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  for (OSDevToolsSession* session : sessions_) {
    session->MaybeTriggerInstrumentationBreakpoint(name);
  }
}

void OSDevToolsAgent::AttachDevToolsSession(
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

  auto session_destroyed_callback = base::BindOnce(
      &OSDevToolsAgent::SessionDestroyed, weak_ptr_factory_.GetWeakPtr());

  auto session = std::make_unique<OSDevToolsSession>(
      v8_env_.get(), *this, debug_command_queue_, session_id,
      client_expects_binary_responses, session_waits_for_debugger,
      std::move(host), io_session_task_runner_, std::move(io_session_receiver),
      std::move(session_destroyed_callback));

  sessions_.insert(session.get());
  // Adding here binds the session_reciever to the session.
  mojom_sessions_.Add(std::move(session), std::move(session_receiver));
}

void OSDevToolsAgent::InspectElement(const ::gfx::Point& point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();  // Should not be used with this.
}

void OSDevToolsAgent::ReportChildTargets(bool report,
                                         bool wait_for_debugger,
                                         ReportChildTargetsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  NOTIMPLEMENTED();  // Should not be used with this.
}

void OSDevToolsAgent::runMessageLoopOnPause(int context_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // NO-OP --- TODO(b/290815208).
}

void OSDevToolsAgent::quitMessageLoopOnPause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  debug_command_queue_->QuitPauseForDebugger();
}
void OSDevToolsAgent::runIfWaitingForDebugger(int context_group_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // NO-OP --- TODO(b/290815208).
}

void OSDevToolsAgent::SessionDestroyed(OSDevToolsSession* session) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  // Pointer cleanup.
  sessions_.erase(session);
}

}  // namespace ax
