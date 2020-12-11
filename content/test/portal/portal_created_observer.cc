// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/portal/portal_created_observer.h"

#include <utility>
#include "base/run_loop.h"
#include "content/browser/portal/portal.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/test/portal/portal_interceptor_for_testing.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {

PortalCreatedObserver::PortalCreatedObserver(
    RenderFrameHostImpl* render_frame_host_impl)
    : render_frame_host_impl_(render_frame_host_impl) {
  old_impl_ = render_frame_host_impl_->frame_host_receiver_for_testing()
                  .SwapImplForTesting(this);
}

PortalCreatedObserver::~PortalCreatedObserver() {
  render_frame_host_impl_->frame_host_receiver_for_testing().SwapImplForTesting(
      old_impl_);
}

mojom::FrameHost* PortalCreatedObserver::GetForwardingInterface() {
  return render_frame_host_impl_;
}

void PortalCreatedObserver::CreatePortal(
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> portal,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
    CreatePortalCallback callback) {
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::Create(render_frame_host_impl_,
                                          std::move(portal), std::move(client));
  portal_ = portal_interceptor->GetPortal();
  RenderFrameProxyHost* proxy_host = portal_->CreateProxyAndAttachPortal();
  std::move(callback).Run(
      proxy_host->GetRoutingID(),
      proxy_host->frame_tree_node()->current_replication_state(),
      portal_->portal_token(), proxy_host->GetFrameToken(),
      portal_->GetDevToolsFrameToken());

  DidCreatePortal();
}

void PortalCreatedObserver::AdoptPortal(const blink::PortalToken& portal_token,
                                        AdoptPortalCallback callback) {
  Portal* portal = render_frame_host_impl_->FindPortalByToken(portal_token);
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::Create(render_frame_host_impl_, portal);
  portal_ = portal_interceptor->GetPortal();
  RenderFrameProxyHost* proxy_host = portal_->CreateProxyAndAttachPortal();
  std::move(callback).Run(
      proxy_host->GetRoutingID(),
      static_cast<RenderWidgetHostViewBase*>(proxy_host->frame_tree_node()
                                                 ->render_manager()
                                                 ->GetRenderWidgetHostView())
          ->GetFrameSinkId(),
      proxy_host->frame_tree_node()->current_replication_state(),
      proxy_host->GetFrameToken(), portal->GetDevToolsFrameToken());

  DidCreatePortal();
}

Portal* PortalCreatedObserver::WaitUntilPortalCreated() {
  Portal* portal = portal_;
  if (portal) {
    portal_ = nullptr;
    return portal;
  }

  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop.Run();
  run_loop_ = nullptr;

  portal = portal_;
  portal_ = nullptr;
  return portal;
}

void PortalCreatedObserver::DidCreatePortal() {
  DCHECK(portal_);
  if (!created_cb_.is_null())
    std::move(created_cb_).Run(portal_);
  if (run_loop_)
    run_loop_->Quit();
}

}  // namespace content
