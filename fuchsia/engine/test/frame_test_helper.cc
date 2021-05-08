// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test/frame_test_helper.h"

namespace cr_fuchsia {

FrameTestHelper::FrameTestHelper(fuchsia::web::Context* context,
                                 fuchsia::web::CreateFrameParams params)
    : navigation_listener_binding_(&navigation_listener_) {
  context->CreateFrameWithParams(std::move(params), frame_.NewRequest());
  frame_->SetNavigationEventListener(navigation_listener_binding_.NewBinding());
}

FrameTestHelper::FrameTestHelper(fuchsia::web::FrameHost* frame_host,
                                 fuchsia::web::CreateFrameParams params)
    : navigation_listener_binding_(&navigation_listener_) {
  frame_host->CreateFrameWithParams(std::move(params), frame_.NewRequest());
  frame_->SetNavigationEventListener(navigation_listener_binding_.NewBinding());
}

FrameTestHelper::FrameTestHelper(const fuchsia::web::ContextPtr& context,
                                 fuchsia::web::CreateFrameParams params)
    : FrameTestHelper(context.get(), std::move(params)) {}

fuchsia::web::NavigationControllerPtr
FrameTestHelper::GetNavigationController() {
  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  return controller;
}

}  // namespace cr_fuchsia
