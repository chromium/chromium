// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEVTOOLS_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEVTOOLS_SESSION_H_

#include <memory>
#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_session_state.h"
#include "third_party/blink/renderer/core/inspector/protocol/Forward.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8-inspector-protocol.h"

namespace blink {

class DevToolsAgent;
class DocumentLoader;
class InspectorAgent;
class LocalFrame;

class CORE_EXPORT DevToolsSession : public GarbageCollected<DevToolsSession>,
                                    public mojom::blink::DevToolsSession,
                                    public protocol::FrontendChannel,
                                    public v8_inspector::V8Inspector::Channel {
 public:
  DevToolsSession(
      DevToolsAgent*,
      mojo::PendingAssociatedRemote<mojom::blink::DevToolsSessionHost>
          host_remote,
      mojo::PendingAssociatedReceiver<mojom::blink::DevToolsSession>
          main_receiver,
      mojo::PendingReceiver<mojom::blink::DevToolsSession> io_receiver,
      mojom::blink::DevToolsSessionStatePtr reattach_session_state,
      bool client_expects_binary_responses);
  ~DevToolsSession() override;

  void ConnectToV8(v8_inspector::V8Inspector*, int context_group_id);
  v8_inspector::V8InspectorSession* V8Session() { return v8_session_.get(); }

  void Append(InspectorAgent*);
  void Detach();
  void FlushProtocolNotifications();
  void Trace(blink::Visitor*);

  // Core probes.
  void DidStartProvisionalLoad(LocalFrame*);
  void DidFailProvisionalLoad(LocalFrame*);
  void DidCommitLoad(LocalFrame*, DocumentLoader*);

 private:
  class IOSession;

  // mojom::blink::DevToolsSession implementation.
  void DispatchProtocolCommand(
      int call_id,
      const String& method,
      mojom::blink::DevToolsMessagePtr message) override;
  void DispatchProtocolCommandImpl(int call_id,
                                   const String& method,
                                   Vector<uint8_t> message);

  // protocol::FrontendChannel implementation.
  void sendProtocolResponse(
      int call_id,
      std::unique_ptr<protocol::Serializable> message) override;
  void sendProtocolNotification(
      std::unique_ptr<protocol::Serializable> message) override;
  void fallThrough(int call_id,
                   const String& method,
                   const protocol::ProtocolMessage& message) override;
  void flushProtocolNotifications() override;

  // v8_inspector::V8Inspector::Channel implementation.
  void sendResponse(
      int call_id,
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override;

  bool IsDetached();
  void SendProtocolResponse(int call_id, std::vector<uint8_t> message);

  // Converts to JSON if requested by the client.
  blink::mojom::blink::DevToolsMessagePtr FinalizeMessage(
      std::vector<uint8_t> message) const;

  Member<DevToolsAgent> agent_;
  mojo::AssociatedReceiver<mojom::blink::DevToolsSession> receiver_;
  mojo::AssociatedRemote<mojom::blink::DevToolsSessionHost> host_remote_;
  IOSession* io_session_;
  std::unique_ptr<v8_inspector::V8InspectorSession> v8_session_;
  std::unique_ptr<protocol::UberDispatcher> inspector_backend_dispatcher_;
  InspectorSessionState session_state_;
  HeapVector<Member<InspectorAgent>> agents_;
  // Notifications are lazily serialized to shift the overhead we spend away
  // from Javascript code that generates many events (e.g., a loop logging to
  // console on every iteration).
  Vector<base::OnceCallback<std::vector<uint8_t>()>> notification_queue_;
  const bool client_expects_binary_responses_;
  InspectorAgentState v8_session_state_;
  InspectorAgentState::Bytes v8_session_state_cbor_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsSession);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEVTOOLS_SESSION_H_
