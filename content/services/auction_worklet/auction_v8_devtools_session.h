// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_DEVTOOLS_SESSION_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_DEVTOOLS_SESSION_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "third_party/inspector_protocol/crdtp/frontend_channel.h"
#include "v8/include/v8-inspector.h"

namespace auction_worklet {

class AuctionV8Helper;
class DebugCommandQueue;

// Session runs on the V8 thread, and routes messages between the V8 inspector
// and mojo DevToolsSession pipe.
class AuctionV8DevToolsSession : public blink::mojom::DevToolsSession,
                                 public crdtp::FrontendChannel,
                                 public v8_inspector::V8Inspector::Channel {
 public:
  using SessionDestroyedCallback =
      base::OnceCallback<void(AuctionV8DevToolsSession*)>;

  // Prepares to act as a DevToolsSession implementation for the worklet
  // identified with `context_group_id`, sending responses` and notifications
  // back via `host`.
  //
  // Assumes `v8_helper` will own `this` and `debug_command_queue` both.
  //
  // The actual mojo pipe for the DevToolsSession itself is expected to be
  // connected externally.
  //
  // Also creates a receiver for `io_session_receiver` running on
  // `io_session_receiver_sequence`.
  //
  // `v8_helper` is required to outlive this.
  // `on_delete_callback` will be invoked from the destructor.
  //
  AuctionV8DevToolsSession(
      AuctionV8Helper* v8_helper,
      scoped_refptr<DebugCommandQueue> debug_command_queue,
      int context_group_id,
      const std::string& session_id,
      bool client_expects_binary_responses,
      bool session_waits_for_debugger,
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsSessionHost> host,
      scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence,
      mojo::PendingReceiver<blink::mojom::DevToolsSession> io_session_receiver,
      SessionDestroyedCallback on_delete_callback);
  AuctionV8DevToolsSession(const AuctionV8DevToolsSession&) = delete;
  AuctionV8DevToolsSession& operator=(const AuctionV8DevToolsSession&) = delete;
  ~AuctionV8DevToolsSession() override;

  // Creates a callback that will ask V8 to exit a debugger pause and abort
  // further execution. Bound to a weak pointer.
  base::OnceClosure MakeAbortPauseCallback();

  int context_group_id() const { return context_group_id_; }

  // If an instrumentation breakpoint named `name` has been set, asks V8 for
  // execution to be paused at next statement.
  void MaybeTriggerInstrumentationBreakpoint(const std::string& name);

  // Invoked from IOSession via DebugCommandQueue.
  void DispatchProtocolCommandFromIO(int32_t call_id,
                                     const std::string& method,
                                     std::vector<uint8_t> message);

  // DevToolsSession implementation:
  void DispatchProtocolCommand(int32_t call_id,
                               const std::string& method,
                               base::span<const uint8_t> message) override;

  // V8Inspector::Channel implementation:
  void sendResponse(
      int call_id,
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void flushProtocolNotifications() override;

  // FrontendChannel implementation. This is like V8Inspector::Channel but used
  // by fallback_dispatcher_.
  void SendProtocolResponse(
      int call_id,
      std::unique_ptr<crdtp::Serializable> message) override;
  void SendProtocolNotification(
      std::unique_ptr<crdtp::Serializable> message) override;
  void FallThrough(int call_id,
                   crdtp::span<uint8_t> method,
                   crdtp::span<uint8_t> message) override;
  void FlushProtocolNotifications() override;

 private:
  class IOSession;
  class BreakpointHandler;

  void AbortDebuggerPause();

  void SendProtocolResponseImpl(int call_id, std::vector<uint8_t> message);
  void SendNotificationImpl(std::vector<uint8_t> message);

  blink::mojom::DevToolsMessagePtr FinalizeMessage(
      std::vector<uint8_t> message) const;

  const raw_ptr<AuctionV8Helper> v8_helper_;  // owns agent owns this.
  const raw_ptr<DebugCommandQueue>
      debug_command_queue_;  // owned by `v8_helper`.
  const int context_group_id_;
  const std::string session_id_;
  const bool client_expects_binary_responses_;
  mojo::AssociatedRemote<blink::mojom::DevToolsSessionHost> host_;
  SessionDestroyedCallback on_delete_callback_;
  std::unique_ptr<v8_inspector::V8InspectorSession> v8_session_;
  std::unique_ptr<BreakpointHandler> breakpoint_handler_;
  crdtp::UberDispatcher fallback_dispatcher_{this};
  SEQUENCE_CHECKER(v8_sequence_checker_);
  base::WeakPtrFactory<AuctionV8DevToolsSession> weak_ptr_factory_{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_DEVTOOLS_SESSION_H_
