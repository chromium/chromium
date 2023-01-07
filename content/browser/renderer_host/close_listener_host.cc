// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/close_listener_host.h"

#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

CloseListenerHost::CloseListenerHost(RenderFrameHost* render_frame_host)
    : DocumentUserData<CloseListenerHost>(render_frame_host) {}

CloseListenerHost::~CloseListenerHost() = default;

void CloseListenerHost::SetListener(
    mojo::PendingRemote<blink::mojom::CloseListener> listener) {
  if (close_listener_.is_bound())
    close_listener_.reset();
  close_listener_.Bind(std::move(listener));
  // The renderer resets the message pipe when no CloseWatchers are
  // waiting for a Signal().
  close_listener_.reset_on_disconnect();
}

bool CloseListenerHost::SignalIfActive() {
  if (!close_listener_)
    return false;
  close_listener_->Signal();
  return true;
}

DOCUMENT_USER_DATA_KEY_IMPL(CloseListenerHost);

}  // namespace content
