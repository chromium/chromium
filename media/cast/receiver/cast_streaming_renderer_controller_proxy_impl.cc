// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/cast_streaming_renderer_controller_proxy_impl.h"

#include "base/bind.h"

namespace media {
namespace cast {

CastStreamingRendererControllerProxyImpl::
    CastStreamingRendererControllerProxyImpl() = default;

CastStreamingRendererControllerProxyImpl::
    ~CastStreamingRendererControllerProxyImpl() = default;

base::RepeatingCallback<void(
    mojo::PendingAssociatedReceiver<mojom::CastStreamingRendererController>)>
CastStreamingRendererControllerProxyImpl::GetBinder(
    content::RenderFrame* frame) {
  // base::Unretained is safe here because this binder is used only for
  // RenderFrame / RenderFrameHost communication, and as this class is owned by
  // the ContentRendererClient which outlives any such instances.
  return base::BindRepeating(
      &CastStreamingRendererControllerProxyImpl::BindInterface,
      base::Unretained(this), frame);
}

void CastStreamingRendererControllerProxyImpl::OnRenderFrameDestroyed(
    content::RenderFrame* frame) {
  DCHECK(frame);

  per_frame_proxies_.erase(frame);
}

mojo::PendingReceiver<media::mojom::Renderer>
CastStreamingRendererControllerProxyImpl::GetReceiver(
    content::RenderFrame* frame) {
  DCHECK(frame);

  auto it = GetFrameProxy(frame);
  return it->second->GetReceiver();
}

void CastStreamingRendererControllerProxyImpl::BindInterface(
    content::RenderFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::CastStreamingRendererController>
        pending_receiver) {
  DCHECK(frame);
  DCHECK(pending_receiver);

  auto it = GetFrameProxy(frame);
  it->second->BindReceiver(std::move(pending_receiver));
}

CastStreamingRendererControllerProxyImpl::RenderFrameMap::iterator
CastStreamingRendererControllerProxyImpl::GetFrameProxy(
    content::RenderFrame* frame) {
  // Inset a new FrameProxy only if there is not yet one associated with
  // |frame|.
  auto pair = per_frame_proxies_.emplace(frame, std::unique_ptr<FrameProxy>());

  // If the item was inserted (so it did not yet exist), swap the empty
  // unique_ptr with a "real" one.
  if (pair.second) {
    // base::Unretained is safe here because this callback is used only for
    // RenderFrame instances, and as this class is owned by the
    // ContentRendererClient which outlives any such instances.
    auto real_frame_proxy = std::make_unique<FrameProxy>(base::BindRepeating(
        &CastStreamingRendererControllerProxyImpl::OnRenderFrameDestroyed,
        base::Unretained(this), frame));
    std::unique_ptr<FrameProxy>& emplaced_instance = pair.first->second;
    emplaced_instance.swap(real_frame_proxy);
  }

  return pair.first;
}

CastStreamingRendererControllerProxyImpl::FrameProxy::FrameProxy(
    base::RepeatingCallback<void()> disconnection_handler)
    : renderer_process_pending_receiver_(
          renderer_process_remote_.InitWithNewPipeAndPassReceiver()),
      browser_process_receiver_(this),
      on_mojo_disconnection_(std::move(disconnection_handler)) {}

CastStreamingRendererControllerProxyImpl::FrameProxy::~FrameProxy() = default;

void CastStreamingRendererControllerProxyImpl::FrameProxy::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::CastStreamingRendererController>
        pending_receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  browser_process_receiver_.Bind(std::move(pending_receiver));
  browser_process_receiver_.set_disconnect_handler(on_mojo_disconnection_);
}

mojo::PendingReceiver<media::mojom::Renderer>
CastStreamingRendererControllerProxyImpl::FrameProxy::GetReceiver() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(renderer_process_pending_receiver_);
  return std::move(renderer_process_pending_receiver_);
}

void CastStreamingRendererControllerProxyImpl::FrameProxy::
    SetPlaybackController(mojo::PendingReceiver<media::mojom::Renderer>
                              browser_process_renderer_controls) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(mojo::FusePipes(std::move(browser_process_renderer_controls),
                        std::move(renderer_process_remote_)));
}

}  // namespace cast
}  // namespace media
