// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_DEVTOOLS_OS_DEVTOOLS_AGENT_H_
#define SERVICES_ACCESSIBILITY_FEATURES_DEVTOOLS_OS_DEVTOOLS_AGENT_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "v8/include/v8-inspector.h"

namespace ax {

class V8Environment;
class OSDevToolsSession;
class DebugCommandQueue;

// This class acts as the receiver for a Devtools agent host. Specifically one
// that uses AccessibilityServiceDevToolsDelegate.
class OSDevToolsAgent : public blink::mojom::DevToolsAgent,
                        public v8_inspector::V8InspectorClient {
 public:
  OSDevToolsAgent(
      V8Environment& v8_env,
      scoped_refptr<base::SequencedTaskRunner> io_session_task_runner);
  OSDevToolsAgent(const OSDevToolsAgent&) = delete;
  OSDevToolsAgent& operator=(const OSDevToolsAgent&) = delete;
  ~OSDevToolsAgent() override;

  // Connects an incoming Mojo debugging connection to endpoint `agent`.
  void Connect(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent);

  // If any session debugging `context_group_id` has an instrumentation
  // breakpoint named `name` set, asks for execution to be paused at next
  // statement.
  void MaybeTriggerInstrumentationBreakpoint(const std::string& name);

  // Creates the V8Session that OSDevToolsSession communicates with.
  std::unique_ptr<v8_inspector::V8InspectorSession> ConnectSession(
      OSDevToolsSession* session,
      bool session_waits_for_debugger);

 private:
  // DevToolsAgent implementation.
  void AttachDevToolsSession(
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsSessionHost> host,
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsSession>
          session_receiver,
      mojo::PendingReceiver<blink::mojom::DevToolsSession> io_session_receiver,
      blink::mojom::DevToolsSessionStatePtr reattach_session_state,
      bool client_expects_binary_responses,
      bool client_is_trusted,
      const std::string& session_id,
      bool session_waits_for_debugger) override;
  void InspectElement(const ::gfx::Point& point) override;
  void ReportChildTargets(bool report,
                          bool wait_for_debugger,
                          ReportChildTargetsCallback callback) override;
  // V8InspectorClient implementation.

  // TODO(b/290815208): Implement after source files are able to be loaded.
  void runMessageLoopOnPause(int context_group_id) override;
  void quitMessageLoopOnPause() override;
  // TODO(b/290815208): Implement
  void runIfWaitingForDebugger(int context_group_id) override;

  // Called via ~OSDevToolsSession.
  void SessionDestroyed(OSDevToolsSession* session);

  const raw_ref<V8Environment> v8_env_;
  const scoped_refptr<DebugCommandQueue> debug_command_queue_;
  const scoped_refptr<base::SequencedTaskRunner> io_session_task_runner_;
  const std::unique_ptr<v8_inspector::V8Inspector> v8_inspector_;

  // Mojo pipes connected to `this`.
  mojo::AssociatedReceiver<blink::mojom::DevToolsAgent> receiver_{this};

  // All OSDevToolsSession objects have their lifetime limited by their mojo
  // pipes. Thus this class always outlives the sessions.
  mojo::UniqueAssociatedReceiverSet<blink::mojom::DevToolsSession>
      mojom_sessions_;
  // These pointers are kept around for access to the session for non-mojo
  // related tasks. The session destroyed callback ensures these are erased when
  // a session is disconnected.
  std::set<raw_ptr<OSDevToolsSession, SetExperimental>> sessions_;

  SEQUENCE_CHECKER(v8_sequence_checker_);
  base::WeakPtrFactory<OSDevToolsAgent> weak_ptr_factory_{this};
};

}  // namespace ax
#endif  // SERVICES_ACCESSIBILITY_FEATURES_DEVTOOLS_OS_DEVTOOLS_AGENT_H_
