// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/update_user_activation_state_interceptor.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

UpdateUserActivationStateInterceptor::UpdateUserActivationStateInterceptor(
    RenderFrameHost* render_frame_host)
    : swapped_impl_(RenderFrameHostImpl::From(render_frame_host)
                        ->local_frame_host_receiver_for_testing(),
                    this) {}

UpdateUserActivationStateInterceptor::~UpdateUserActivationStateInterceptor() =
    default;

void UpdateUserActivationStateInterceptor::set_quit_handler(
    base::OnceClosure handler) {
  quit_handler_ = std::move(handler);
}

blink::mojom::LocalFrameHost*
UpdateUserActivationStateInterceptor::GetForwardingInterface() {
  return swapped_impl_.old_impl();
}

void UpdateUserActivationStateInterceptor::UpdateUserActivationState(
    blink::mojom::UserActivationUpdateType update_type,
    blink::mojom::UserActivationNotificationType notification_type) {
  update_user_activation_state_ = true;
  if (quit_handler_) {
    std::move(quit_handler_).Run();
  }
  GetForwardingInterface()->UpdateUserActivationState(update_type,
                                                      notification_type);
}

}  // namespace content
