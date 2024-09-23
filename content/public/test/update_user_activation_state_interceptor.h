// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_UPDATE_USER_ACTIVATION_STATE_INTERCEPTOR_H_
#define CONTENT_PUBLIC_TEST_UPDATE_USER_ACTIVATION_STATE_INTERCEPTOR_H_

#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"

namespace content {

class RenderFrameHost;

class UpdateUserActivationStateInterceptor
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  explicit UpdateUserActivationStateInterceptor(
      content::RenderFrameHost* render_frame_host);
  UpdateUserActivationStateInterceptor(
      const UpdateUserActivationStateInterceptor&) = delete;
  UpdateUserActivationStateInterceptor& operator=(
      const UpdateUserActivationStateInterceptor&) = delete;
  ~UpdateUserActivationStateInterceptor() override;

  void set_quit_handler(base::OnceClosure handler);
  bool update_user_activation_state() { return update_user_activation_state_; }

  blink::mojom::LocalFrameHost* GetForwardingInterface() override;
  void UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) override;

 private:
  mojo::test::ScopedSwapImplForTesting<blink::mojom::LocalFrameHost>
      swapped_impl_;
  base::OnceClosure quit_handler_;
  bool update_user_activation_state_ = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_UPDATE_USER_ACTIVATION_STATE_INTERCEPTOR_H_
