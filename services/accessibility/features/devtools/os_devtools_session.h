// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_DEVTOOLS_OS_DEVTOOLS_SESSION_H_
#define SERVICES_ACCESSIBILITY_FEATURES_DEVTOOLS_OS_DEVTOOLS_SESSION_H_

#include <cstdint>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "services/accessibility/features/devtools/os_devtools_agent.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "third_party/inspector_protocol/crdtp/dispatch.h"
#include "third_party/inspector_protocol/crdtp/frontend_channel.h"
#include "v8/include/v8-inspector.h"

namespace ax {
class V8Environment;
class DebugCommandQueue;

// This is a devtools session implementation for the accessibility service. It
// is owned by OSDevToolsAgent.
class OSDevToolsSession : public blink::mojom::DevToolsSession,
                          public crdtp::FrontendChannel,
                          public v8_inspector::V8Inspector::Channel {
 public:
  using SessionDestroyedCallback = base::OnceCallback<void(OSDevToolsSession*)>;

  OSDevToolsSession(
      V8Environment& v8_env,
      OSDevToolsAgent& agent,
      const scoped_refptr<DebugCommandQueue> debug_command_queue,
      const std::string& session_id,
      bool client_expects_binary_response,
      bool session_waits_for_debugger,
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsSessionHost> host,
      scoped_refptr<base::SequencedTaskRunner> io_session_receiver_sequence,
      mojo::PendingReceiver<blink::mojom::DevToolsSession> io_session_receiver,
      SessionDestroyedCallback on_delete_callback);

  OSDevToolsSession(const OSDevToolsSession&) = delete;
  OSDevToolsSession& operator=(const OSDevToolsSession&) = delete;
  ~OSDevToolsSession() override;

  // Creates a callback that will ask V8 to exit a debugger pause and abort
  // further execution. Bound to a weak pointer.
  base::OnceClosure MakeAbortPauseCallback();

  // If an instrumentation breakpoint named `name` has been set, asks V8 for
  // execution to be paused at next statement.
  void MaybeTriggerInstrumentationBreakpoint(const std::string& name);

  // Invoked from IOSession via DebugCommandQueue.
  void DispatchProtocolCommandFromIO(int32_t call_id,
                                     const std::string& method,
                                     std::vector<uint8_t> message);

  // blink::mojom::DevToolsSession
  void DispatchProtocolCommand(int32_t call_id,
                               const std::string& method,
                               base::span<const uint8_t> message) override;

  // V8Inspector::Channel
  void sendResponse(
      int call_id,
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void flushProtocolNotifications() override;

  // FrontendChannel.
  // This is like V8Inspector::Channel but used  by fallback_dispatcher_.
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

  void AbortDebuggerPause();

  void SendProtocolResponseImpl(int call_id, std::vector<uint8_t> message);
  void SendNotificationImpl(std::vector<uint8_t> message);

  blink::mojom::DevToolsMessagePtr FinalizeMessage(
      std::vector<uint8_t> message) const;

  const raw_ref<V8Environment>
      v8_env_;  // Environment owns agent which owns `this`.
  const raw_ref<DebugCommandQueue> debug_command_queue_;  // owned by agent.
  const std::string session_id_;
  const bool client_expects_binary_responses_;
  mojo::AssociatedRemote<blink::mojom::DevToolsSessionHost> host_;
  mojo::AssociatedReceiver<blink::mojom::DevToolsSession> receiver_{this};
  SessionDestroyedCallback on_delete_callback_;
  std::unique_ptr<v8_inspector::V8InspectorSession> v8_session_;
  crdtp::UberDispatcher fallback_dispatcher_{this};
  SEQUENCE_CHECKER(v8_sequence_checker_);
  base::WeakPtrFactory<OSDevToolsSession> weak_ptr_factory_{this};
};
}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_DEVTOOLS_OS_DEVTOOLS_SESSION_H_
