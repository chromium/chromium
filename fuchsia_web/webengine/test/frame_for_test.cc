// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/frame_for_test.h"

#include "fuchsia_web/common/test/test_navigation_listener.h"

// static
FrameForTest FrameForTest::Create(fuchsia::web::Context* context,
                                  fuchsia::web::CreateFrameParams params) {
  FrameForTest result;
  context->CreateFrameWithParams(std::move(params), result.frame_.NewRequest());
  result.CreateAndAttachNavigationListener({});
  result->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::DEBUG);
  return result;
}

// static
FrameForTest FrameForTest::Create(fuchsia::web::FrameHost* frame_host,
                                  fuchsia::web::CreateFrameParams params) {
  FrameForTest result;
  frame_host->CreateFrameWithParams(std::move(params),
                                    result.frame_.NewRequest());
  result.CreateAndAttachNavigationListener({});
  result->SetJavaScriptLogLevel(fuchsia::web::ConsoleLogLevel::DEBUG);
  return result;
}

// static
FrameForTest FrameForTest::Create(const fuchsia::web::ContextPtr& context,
                                  fuchsia::web::CreateFrameParams params) {
  return Create(context.get(), std::move(params));
}

// static
FrameForTest FrameForTest::Create(const fuchsia::web::FrameHostPtr& frame_host,
                                  fuchsia::web::CreateFrameParams params) {
  return Create(frame_host.get(), std::move(params));
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

void FrameForTest::CreateAndAttachNavigationListener(
    fuchsia::web::NavigationEventListenerFlags flags) {
  navigation_listener_ = std::make_unique<TestNavigationListener>();
  navigation_listener_binding_ =
      std::make_unique<fidl::Binding<fuchsia::web::NavigationEventListener>>(
          navigation_listener_.get());
  frame_->SetNavigationEventListener2(
      navigation_listener_binding_->NewBinding(), flags);
}
