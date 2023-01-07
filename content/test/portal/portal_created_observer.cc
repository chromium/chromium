// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/portal/portal_created_observer.h"

#include <utility>
#include "base/run_loop.h"
#include "content/browser/portal/portal.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/test/portal/portal_interceptor_for_testing.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace content {

PortalCreatedObserver::PortalCreatedObserver(
    RenderFrameHostImpl* render_frame_host_impl)
    : render_frame_host_impl_(render_frame_host_impl),
      swapped_impl_(
          render_frame_host_impl_->local_frame_host_receiver_for_testing(),
          this) {}

PortalCreatedObserver::~PortalCreatedObserver() = default;

blink::mojom::LocalFrameHost* PortalCreatedObserver::GetForwardingInterface() {
  return render_frame_host_impl_;
}

void PortalCreatedObserver::CreatePortal(
    mojo::PendingAssociatedReceiver<blink::mojom::Portal> portal,
    mojo::PendingAssociatedRemote<blink::mojom::PortalClient> client,
    blink::mojom::RemoteFrameInterfacesFromRendererPtr remote_frame_interfaces,
    CreatePortalCallback callback) {
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::Create(render_frame_host_impl_,
                                          std::move(portal), std::move(client));
  portal_ = portal_interceptor->GetPortal();
  RenderFrameProxyHost* proxy_host =
      portal_->CreateProxyAndAttachPortal(std::move(remote_frame_interfaces));
  std::move(callback).Run(
      proxy_host->frame_tree_node()->current_replication_state().Clone(),
      portal_->portal_token(), proxy_host->GetFrameToken(),
      portal_->GetDevToolsFrameToken());

  DidCreatePortal();
}

void PortalCreatedObserver::AdoptPortal(
    const blink::PortalToken& portal_token,
    blink::mojom::RemoteFrameInterfacesFromRendererPtr remote_frame_interfaces,
    AdoptPortalCallback callback) {
  Portal* portal = render_frame_host_impl_->FindPortalByToken(portal_token);
  PortalInterceptorForTesting* portal_interceptor =
      PortalInterceptorForTesting::Create(render_frame_host_impl_, portal);
  portal_ = portal_interceptor->GetPortal();
  RenderFrameProxyHost* proxy_host =
      portal_->CreateProxyAndAttachPortal(std::move(remote_frame_interfaces));
  std::move(callback).Run(
      proxy_host->frame_tree_node()->current_replication_state().Clone(),
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
    std::move(created_cb_).Run(portal_.get());
  if (run_loop_)
    run_loop_->Quit();
}

}  // namespace content
