// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/test/frame_for_test.h"

#include "fuchsia/base/test/test_navigation_listener.h"

namespace cr_fuchsia {

// static
FrameForTest FrameForTest::Create(fuchsia::web::Context* context,
                                  fuchsia::web::CreateFrameParams params) {
  FrameForTest result;
  context->CreateFrameWithParams(std::move(params), result.frame_.NewRequest());
  result.CreateAndAttachNavigationListener();
  return result;
}

// static
FrameForTest FrameForTest::Create(fuchsia::web::FrameHost* frame_host,
                                  fuchsia::web::CreateFrameParams params) {
  FrameForTest result;
  frame_host->CreateFrameWithParams(std::move(params),
                                    result.frame_.NewRequest());
  result.CreateAndAttachNavigationListener();
  return result;
}

// static
FrameForTest FrameForTest::Create(const fuchsia::web::ContextPtr& context,
                                  fuchsia::web::CreateFrameParams params) {
  return Create(context.get(), std::move(params));
}

FrameForTest::FrameForTest() = default;

FrameForTest::FrameForTest(FrameForTest&&) = default;

FrameForTest& FrameForTest::operator=(FrameForTest&&) = default;

FrameForTest::~FrameForTest() = default;

fuchsia::web::NavigationControllerPtr FrameForTest::GetNavigationController() {
  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  return controller;
}

void FrameForTest::CreateAndAttachNavigationListener() {
  navigation_listener_ = std::make_unique<TestNavigationListener>();
  navigation_listener_binding_ =
      std::make_unique<fidl::Binding<fuchsia::web::NavigationEventListener>>(
          navigation_listener_.get());
  frame_->SetNavigationEventListener(
      navigation_listener_binding_->NewBinding());
}

}  // namespace cr_fuchsia
