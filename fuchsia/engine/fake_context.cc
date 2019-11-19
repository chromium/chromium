// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/fake_context.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

FakeFrame::FakeFrame(fidl::InterfaceRequest<fuchsia::web::Frame> request)
    : binding_(this, std::move(request)) {
  binding_.set_error_handler([this](zx_status_t status) {
    ZX_CHECK(status == ZX_ERR_PEER_CLOSED, status);
    delete this;
  });
}

FakeFrame::~FakeFrame() = default;

void FakeFrame::GetNavigationController(
    fidl::InterfaceRequest<fuchsia::web::NavigationController> controller) {
  if (navigation_controller_) {
    navigation_controller_bindings_.AddBinding(navigation_controller_,
                                               std::move(controller));
  }
}

void FakeFrame::SetNavigationEventListener(
    fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener) {
  listener_.Bind(std::move(listener));
  if (on_set_listener_callback_)
    std::move(on_set_listener_callback_).Run();
}

void FakeFrame::NotImplemented_(const std::string& name) {
  NOTREACHED() << name;
}

FakeContext::FakeContext() = default;
FakeContext::~FakeContext() = default;

void FakeContext::CreateFrame(
    fidl::InterfaceRequest<fuchsia::web::Frame> frame_request) {
  FakeFrame* new_frame = new FakeFrame(std::move(frame_request));
  if (on_create_frame_callback_)
    on_create_frame_callback_.Run(new_frame);

  // |new_frame| owns itself, so we intentionally leak the pointer.
}

void FakeContext::NotImplemented_(const std::string& name) {
  NOTREACHED() << name;
}
