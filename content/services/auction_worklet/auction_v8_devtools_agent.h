// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_DEVTOOLS_AGENT_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_DEVTOOLS_AGENT_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "v8/include/v8-inspector.h"

namespace auction_worklet {

class AuctionV8Helper;
class AuctionV8DevToolsSession;
class DebugCommandQueue;

// Implementation of blink.mojom.DevToolsAgent for things run via
// AuctionV8Helper.
//
// Responsible for hooking up DevTools to V8 for auction worklets. Lives
// entirely on the V8 thread, including receiving Mojo messages there, though
// creates DevTools IO session receivers on `io_session_receiver_sequence`,
// as they are required to be on a different thread for use when the V8
// thread is busy.
//
// Receiver for per-context group blink::mojom::DevToolsAgent pipes.
// Creates/manages the lifetimes of the blink::mojom::DevToolsSessions. Also
// serves as the v8_inspector::V8InspectorClient  to handle pause/resume calls
// received back from V8.
//
// To summarize, the thread split is as follows:
//
// Mojo thread:
// - AuctionWorkletService & its Mojo interface
// - BidderWorklet and SellerWorklet objects & their Mojo interfaces
// - IOSession objects & their Mojo interfaces
//
// V8 thread:
// - V8 parsing and running the worklet JavaScript.
// - AuctionV8DevToolsAgent and its mojo
// - AuctionV8DevToolsSession and its mojo
class AuctionV8DevToolsAgent : public blink::mojom::DevToolsAgent,
                               public v8_inspector::V8InspectorClient {
 public:
  // `v8_helper` is expected to own `this`.
  // `debug_command_queue` is expected to be owned by `v8_helper`.
  // `io_session_receiver_sequence` must be distinct from
  // `v8_helper->v8_runner()`, and be able to grab mutexes (for short duration)
  // and handle a Mojo connection.
  AuctionV8DevToolsAgent(
      AuctionV8Helper* v8_helper,
      scoped_refptr<DebugCommandQueue> debug_command_queue,
      scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence);
  AuctionV8DevToolsAgent(const AuctionV8DevToolsAgent&) = delete;
  AuctionV8DevToolsAgent& operator=(const AuctionV8DevToolsAgent&) = delete;
  ~AuctionV8DevToolsAgent() override;

  // Connects an incoming Mojo debugging connection to endpoint `agent`,
  // expecting to debug things associated in the V8Helper with
  // `context_group_id`.
  void Connect(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent,
      int context_group_id);

  // If any session debugging `context_group_id` has an instrumentation
  // breakpoint named `name` set, asks for execution to be paused at next
  // statement.
  void MaybeTriggerInstrumentationBreakpoint(int context_group_id,
                                             const std::string& name);

  // Cleans up all state associated with connections, so the v8 inspector can be
  // safely deleted.
  void DestroySessions();

 private:
  struct ContextGroupInfo {
    ContextGroupInfo();
    ~ContextGroupInfo();

    // Owned by `sessions_` in the AuctionV8DevToolsAgent object; stale entries
    // removed by its SessionDestroyed().
    std::set<raw_ptr<AuctionV8DevToolsSession, SetExperimental>> sessions;
  };

  AuctionV8Helper* v8_helper() { return v8_helper_; }

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
  // TODO(morlovich): Implement consoleAPIMessage and currentTimeMS and replace
  // our limited hand-rolled console implementation.
  void runMessageLoopOnPause(int context_group_id) override;
  void quitMessageLoopOnPause() override;
  void runIfWaitingForDebugger(int context_group_id) override;

  // Called via ~AuctionV8DevToolsSession.
  void SessionDestroyed(AuctionV8DevToolsSession* session);

  const raw_ptr<AuctionV8Helper> v8_helper_;  // owns this.
  const scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence_;

  // Mojo pipes connected to `this`, and context group IDs associated with them.
  mojo::AssociatedReceiverSet<blink::mojom::DevToolsAgent, int> receivers_;

  // All AuctionV8DevToolsSession objects have their lifetime limited by their
  // pipes and `this`.
  mojo::UniqueAssociatedReceiverSet<blink::mojom::DevToolsSession> sessions_;

  // Context groups with live AuctionV8DevToolsSessions. Each entry is created
  // when a blink::mojom::DevToolsAgent in `receivers_` receives a
  // AttachDevToolsSession() call and there are no live sessions associated with
  // its context group ID. Keyed by context group ID.
  //
  // Empty entries are pruned by SessionDestroyed, which is called from
  // ~AuctionV8DevToolsSession (via a callback).
  std::map<int, ContextGroupInfo> context_groups_;

  // Owned by `v8_helper` which owns `this`.
  const raw_ptr<DebugCommandQueue> debug_command_queue_;
  bool paused_ = false;

  SEQUENCE_CHECKER(v8_sequence_checker_);
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_DEVTOOLS_AGENT_H_
