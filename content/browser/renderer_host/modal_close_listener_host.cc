// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/modal_close_listener_host.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

ModalCloseListenerHost::ModalCloseListenerHost(
    RenderFrameHost* render_frame_host) {}

ModalCloseListenerHost::~ModalCloseListenerHost() = default;

void ModalCloseListenerHost::SetListener(
    mojo::PendingRemote<blink::mojom::ModalCloseListener> listener) {
  if (modal_close_listener_.is_bound())
    modal_close_listener_.reset();
  modal_close_listener_.Bind(std::move(listener));
  // The renderer resets the message pipe when no ModalCloseWatchers are
  // waiting for a Signal().
  modal_close_listener_.reset_on_disconnect();
}

bool ModalCloseListenerHost::SignalIfActive() {
  if (!modal_close_listener_)
    return false;
  modal_close_listener_->Signal();
  return true;
}

RENDER_DOCUMENT_HOST_USER_DATA_KEY_IMPL(ModalCloseListenerHost)

}  // namespace content
