// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/direct_sockets/direct_sockets_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

DirectSocketsServiceImpl::PermissionCallback&
GetPermissionCallbackForTesting() {
  static base::NoDestructor<DirectSocketsServiceImpl::PermissionCallback>
      callback;
  return *callback;
}

}  // namespace

DirectSocketsServiceImpl::DirectSocketsServiceImpl(RenderFrameHost& frame_host)
    : WebContentsObserver(WebContents::FromRenderFrameHost(&frame_host)),
      frame_host_(&frame_host) {}

// static
void DirectSocketsServiceImpl::CreateForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::DirectSocketsService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<DirectSocketsServiceImpl>(*render_frame_host),
      std::move(receiver));
}

void DirectSocketsServiceImpl::OpenTcpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    OpenTcpSocketCallback callback) {
  if (!options) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }
  net::Error result = EnsurePermission(*options);

  // TODO(crbug.com/1119681): Collect metrics for usage and permission checks

  if (result == net::OK) {
    // TODO(crbug.com/905818): GetNetworkContext()->CreateTCPConnectedSocket
    GetNetworkContext();
    NOTIMPLEMENTED();
  }

  std::move(callback).Run(result);
}

void DirectSocketsServiceImpl::OpenUdpSocket(
    blink::mojom::DirectSocketOptionsPtr options,
    OpenUdpSocketCallback callback) {
  if (!options) {
    mojo::ReportBadMessage("Invalid request to open socket");
    return;
  }
  net::Error result = EnsurePermission(*options);

  // TODO(crbug.com/1119681): Collect metrics for usage and permission checks

  if (result == net::OK) {
    // TODO(crbug.com/1119620): GetNetworkContext()->CreateUDPSocket
    GetNetworkContext();
    NOTIMPLEMENTED();
  }

  std::move(callback).Run(result);
}

// static
void DirectSocketsServiceImpl::SetPermissionCallbackForTesting(
    PermissionCallback callback) {
  GetPermissionCallbackForTesting() = std::move(callback);
}

void DirectSocketsServiceImpl::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  if (render_frame_host == frame_host_)
    frame_host_ = nullptr;
}

void DirectSocketsServiceImpl::WebContentsDestroyed() {
  frame_host_ = nullptr;
}

net::Error DirectSocketsServiceImpl::EnsurePermission(
    const blink::mojom::DirectSocketOptions& options) {
  DCHECK(base::FeatureList::IsEnabled(features::kDirectSockets));

  if (!frame_host_)
    return net::ERR_CONTEXT_SHUT_DOWN;

  if (GetPermissionCallbackForTesting())
    return GetPermissionCallbackForTesting().Run(options);

  if (options.send_buffer_size < 0 || options.receive_buffer_size < 0)
    return net::ERR_INVALID_ARGUMENT;

  // TODO(crbug.com/1119662): Check for enterprise software policies.
  // TODO(crbug.com/1119659): Check permissions policy.
  // TODO(crbug.com/1119600): Implement rate limiting.

  if (options.remote_port == 443) {
    // TODO(crbug.com/1119601): Issue a CORS preflight request.
    return net::ERR_UNSAFE_PORT;
  }

  // EnsurePermission() will need to become asynchronous:
  // TODO(crbug.com/1119597): Show consent dialog
  // TODO(crbug.com/1119661): Reject hostnames that resolve to non-public
  // addresses.

  return net::ERR_NOT_IMPLEMENTED;
}

network::mojom::NetworkContext* DirectSocketsServiceImpl::GetNetworkContext() {
  return frame_host_->GetStoragePartition()->GetNetworkContext();
}

}  // namespace content
