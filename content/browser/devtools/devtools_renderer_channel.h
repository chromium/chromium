// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_RENDERER_CHANNEL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_RENDERER_CHANNEL_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace gfx {
class Point;
}

namespace content {

class DevToolsAgentHostImpl;
class DevToolsSession;
class RenderFrameHostImpl;
class WorkerOrWorkletDevToolsAgentHost;

// This class encapsulates a connection to blink::mojom::DevToolsAgent
// in the renderer (either RenderFrame or some kind of worker).
// When the renderer changes (e.g. worker restarts or a new RenderFrame
// is used for the frame), different DevToolsAgentHostImpl subclasses
// retrieve a new blink::mojom::DevToolsAgent pointer, and this channel
// starts using it for all existing and future sessions.
class DevToolsRendererChannel : public blink::mojom::DevToolsAgentHost {
 public:
  explicit DevToolsRendererChannel(DevToolsAgentHostImpl* owner);

  DevToolsRendererChannel(const DevToolsRendererChannel&) = delete;
  DevToolsRendererChannel& operator=(const DevToolsRendererChannel&) = delete;

  ~DevToolsRendererChannel() override;

  // Dedicated workers use non-associated version,
  // while frames and other workers use DevToolsAgent associated
  // with respective control interfraces. See mojom for details.
  void SetRenderer(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver,
      int process_id,
      base::OnceClosure connection_error = base::NullCallback());
  void SetRendererAssociated(
      mojo::PendingAssociatedRemote<blink::mojom::DevToolsAgent> agent_remote,
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgentHost>
          host_receiver,
      int process_id,
      RenderFrameHostImpl* frame_host);
  void AttachSession(DevToolsSession* session);
  void InspectElement(const gfx::Point& point);
  using GetUniqueFormCallback = base::OnceCallback<void(uint64_t)>;
  void ForceDetachWorkerSessions();

  using ChildTargetCreatedCallback =
      base::RepeatingCallback<void(DevToolsAgentHostImpl*,
                                   bool waiting_for_debugger)>;
  void SetReportChildTargets(ChildTargetCreatedCallback report_callback,
                             bool wait_for_debugger,
                             base::OnceClosure completion_callback);

 private:
  // blink::mojom::DevToolsAgentHost implementation.
  void ChildTargetCreated(
      mojo::PendingRemote<blink::mojom::DevToolsAgent> worker_devtools_agent,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost> host_receiver,
      const GURL& url,
      const std::string& name,
      const base::UnguessableToken& devtools_worker_token,
      bool waiting_for_debugger,
      blink::mojom::DevToolsExecutionContextType context_type) override;
  void ChildTargetDestroyed(DevToolsAgentHostImpl*);
  void MainThreadDebuggerPaused() override;
  void MainThreadDebuggerResumed() override;
  void BringToForeground() override;

  void CleanupConnection();
  void SetRendererInternal(blink::mojom::DevToolsAgent* agent,
                           int process_id,
                           RenderFrameHostImpl* frame_host,
                           bool force_using_io);
  void ReportChildTargetsCallback();

  raw_ptr<DevToolsAgentHostImpl> owner_;
  mojo::Receiver<blink::mojom::DevToolsAgentHost> receiver_{this};
  mojo::AssociatedReceiver<blink::mojom::DevToolsAgentHost>
      associated_receiver_{this};
  mojo::Remote<blink::mojom::DevToolsAgent> agent_remote_;
  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> associated_agent_remote_;
  int process_id_;
  raw_ptr<RenderFrameHostImpl> frame_host_ = nullptr;
  base::flat_set<raw_ptr<WorkerOrWorkletDevToolsAgentHost, CtnExperimental>>
      child_targets_;
  ChildTargetCreatedCallback child_target_created_callback_;
  bool wait_for_debugger_ = false;
  base::OnceClosure set_report_completion_callback_;
  base::WeakPtrFactory<DevToolsRendererChannel> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_RENDERER_CHANNEL_H_
