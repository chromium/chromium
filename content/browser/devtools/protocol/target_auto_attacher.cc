// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/target_auto_attacher.h"

#include "base/auto_reset.h"
#include "base/containers/queue.h"
#include "content/browser/devtools/devtools_renderer_channel.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {
namespace protocol {

TargetAutoAttacher::TargetAutoAttacher() = default;
TargetAutoAttacher::~TargetAutoAttacher() = default;

void TargetAutoAttacher::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);
  DCHECK(!delegate_);
  delegate_ = delegate;
}

void TargetAutoAttacher::SetRenderFrameHost(
    RenderFrameHostImpl* render_frame_host) {
  DCHECK(!render_frame_host);
}

DevToolsAgentHost* TargetAutoAttacher::AutoAttachToFrame(
    NavigationRequest* navigation_request) {
  return AutoAttachToFrame(navigation_request, wait_for_debugger_on_start_);
}

DevToolsAgentHost* TargetAutoAttacher::AutoAttachToFrame(
    NavigationRequest* navigation_request,
    bool wait_for_debugger_on_start) {
  if (!auto_attach())
    return nullptr;

  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  RenderFrameHostImpl* new_host = navigation_request->GetRenderFrameHost();

  // |new_host| can be nullptr for navigation that doesn't commmit
  // (e.g. download). Skip possibly detaching the old agent host so the DevTools
  // message logged via the old RFH can be seen.
  if (!new_host)
    return nullptr;

  scoped_refptr<DevToolsAgentHost> agent_host =
      RenderFrameDevToolsAgentHost::FindForDangling(frame_tree_node);

  bool is_portal_main_frame =
      frame_tree_node->IsMainFrame() &&
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(new_host))
          ->IsPortal();
  bool needs_host_attached =
      new_host->is_local_root_subframe() || is_portal_main_frame;

  if (needs_host_attached) {
    if (!agent_host) {
      agent_host =
          RenderFrameDevToolsAgentHost::CreateForLocalRootOrPortalNavigation(
              navigation_request);
    }
    return delegate()->AutoAttach(agent_host.get(),
                                  wait_for_debugger_on_start) &&
                   wait_for_debugger_on_start
               ? agent_host.get()
               : nullptr;
  }

  if (!agent_host)
    return nullptr;

  // At this point we don't need a host, so we must auto detach if we auto
  // attached earlier.
  delegate_->AutoDetach(agent_host.get());
  return nullptr;
}

void TargetAutoAttacher::UpdateAutoAttach(base::OnceClosure callback) {
  std::move(callback).Run();
}

void TargetAutoAttacher::SetAutoAttach(bool auto_attach,
                                       bool wait_for_debugger_on_start,
                                       base::OnceClosure callback) {
  wait_for_debugger_on_start_ = wait_for_debugger_on_start;
  if (auto_attach == auto_attach_) {
    std::move(callback).Run();
    return;
  }
  auto_attach_ = auto_attach;
  UpdateAutoAttach(std::move(callback));
}

void TargetAutoAttacher::ChildWorkerCreated(DevToolsAgentHostImpl* agent_host,
                                            bool waiting_for_debugger) {
  delegate_->AutoAttach(agent_host, waiting_for_debugger);
}

void TargetAutoAttacher::UpdatePortals() {
  NOTREACHED();
}

void TargetAutoAttacher::DidFinishNavigation(
    NavigationRequest* navigation_request) {
  NOTREACHED();
}

RendererAutoAttacherBase::RendererAutoAttacherBase(
    DevToolsRendererChannel* renderer_channel)
    : renderer_channel_(renderer_channel) {}

RendererAutoAttacherBase::~RendererAutoAttacherBase() = default;

void RendererAutoAttacherBase::UpdateAutoAttach(base::OnceClosure callback) {
  renderer_channel_->SetReportChildWorkers(
      this, auto_attach(), wait_for_debugger_on_start(), std::move(callback));
}

}  // namespace protocol
}  // namespace content
