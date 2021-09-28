// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_DEVTOOLS_AGENT_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_DEVTOOLS_AGENT_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "content/services/auction_worklet/debug_command_queue.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "v8/include/v8-inspector.h"

namespace auction_worklet {

class AuctionV8Helper;
class AuctionV8DevToolsSession;

// Implementation of blink.mojom.DevToolsAgent for things run via
// AuctionV8Helper.
//
// Responsible for hooking up DevTools to V8 for auction worklets. Lives
// entirely on the V8 thread, including receiving Mojo messages there, though
// creates DevTools IO session receivers on `io_session_receiver_sequence`,
// as they are required to be on a different thread for use when the V8
// thread is busy.
//
// Receiver for per-context group blink::mojom::DevToolsAgent pipes. Manages
// per-context group DebugCommandQueues and creates/manages the lifetimes of the
// blink::mojom::DevToolsSessions. Also serves as the
// v8_inspector::V8InspectorClient  to handle pause/resume calls received back
// from V8.
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
  // `io_session_receiver_sequence` must be distinct from
  // `v8_helper->v8_runner()`, and be able to grab mutexes (for short duration)
  // and handle a Mojo connection.
  AuctionV8DevToolsAgent(
      AuctionV8Helper* v8_helper,
      scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence);
  AuctionV8DevToolsAgent(const AuctionV8DevToolsAgent&) = delete;
  AuctionV8DevToolsAgent& operator=(const AuctionV8DevToolsAgent&) = delete;
  ~AuctionV8DevToolsAgent() override;

  // Connects an incoming Mojo debugging connection to endpoint `agent`,
  // expecting to debug things associated in the V8Helper with
  // `context_group_id`.
  void Connect(mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent,
               int context_group_id);

  // Cleans up all state associated with connections, so the v8 inspector can be
  // safely deleted.
  void DestroySessions();

 private:
  struct ContextGroupInfo {
    ContextGroupInfo();
    ~ContextGroupInfo();

    scoped_refptr<DebugCommandQueue> command_queue =
        base::MakeRefCounted<DebugCommandQueue>();

    // Owned by `sessions_` in the AuctionV8DevToolsAgent object; stale entries
    // removed by its SessionDestroyed().
    std::set<AuctionV8DevToolsSession*> sessions;
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
      const std::string& session_id) override;
  void InspectElement(const ::gfx::Point& point) override;
  void ReportChildWorkers(bool report,
                          bool wait_for_debugger,
                          ReportChildWorkersCallback callback) override;

  // V8InspectorClient implementation.
  // TODO(morlovich): Implement consoleAPIMessage and currentTimeMS and replace
  // our limited hand-rolled console implementation.
  void runMessageLoopOnPause(int context_group_id) override;
  void quitMessageLoopOnPause() override;
  void runIfWaitingForDebugger(int context_group_id) override;

  // Called via ~AuctionV8DevToolsSession.
  void SessionDestroyed(AuctionV8DevToolsSession* session);

  AuctionV8Helper* const v8_helper_;  // owns this.
  const scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence_;

  // Mojo pipes connected to `this`, and context group IDs associated with them.
  mojo::ReceiverSet<blink::mojom::DevToolsAgent, int> receivers_;

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

  // Command queue for context group that has currently paused V8 execution.
  // While non-empty, the V8 thread's message loop should be blocked
  // waiting on more commands to be queued, and executing queued commands.
  scoped_refptr<DebugCommandQueue> paused_context_group_queue_;

  SEQUENCE_CHECKER(v8_sequence_checker_);
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_DEVTOOLS_AGENT_H_
