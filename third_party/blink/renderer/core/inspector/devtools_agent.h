// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEVTOOLS_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEVTOOLS_AGENT_H_

#include <memory>

#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "third_party/blink/public/web/devtools_agent.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_session.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class InspectorTaskRunner;

class CORE_EXPORT DevToolsAgent
    : public GarbageCollectedFinalized<DevToolsAgent>,
      public mojom::blink::DevToolsAgent {
 public:
  class Client {
   public:
    virtual ~Client() {}
    virtual InspectorSession* AttachSession(
        InspectorSession::Client*,
        mojom::blink::DevToolsSessionStatePtr reattach_session_state) = 0;
    virtual void DetachSession(InspectorSession*) = 0;
    virtual void InspectElement(const WebPoint&) = 0;
  };

  DevToolsAgent(Client*,
                scoped_refptr<InspectorTaskRunner> inspector_task_runner,
                scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~DevToolsAgent() override;

  void Dispose();
  void FlushProtocolNotifications();
  void BindRequest(mojom::blink::DevToolsAgentHostAssociatedPtrInfo,
                   mojom::blink::DevToolsAgentAssociatedRequest,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  virtual void Trace(blink::Visitor*);

 private:
  class Session;

  // mojom::blink::DevToolsAgent implementation.
  void AttachDevToolsSession(
      mojom::blink::DevToolsSessionHostAssociatedPtrInfo,
      mojom::blink::DevToolsSessionAssociatedRequest main_session,
      mojom::blink::DevToolsSessionRequest io_session,
      mojom::blink::DevToolsSessionStatePtr reattach_session_state) override;
  void InspectElement(const WebPoint& point) override;

  void CleanupConnection();

  Client* client_;
  mojo::AssociatedBinding<mojom::blink::DevToolsAgent> binding_;
  mojom::blink::DevToolsAgentHostAssociatedPtr host_ptr_;
  HeapHashSet<Member<Session>> sessions_;
  scoped_refptr<InspectorTaskRunner> inspector_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_DEVTOOLS_AGENT_H_
