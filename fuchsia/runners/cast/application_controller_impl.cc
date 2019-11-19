// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/application_controller_impl.h"

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

ApplicationControllerImpl::ApplicationControllerImpl(
    fuchsia::web::Frame* frame,
    fidl::InterfaceHandle<chromium::cast::ApplicationControllerReceiver>
        receiver)
    : binding_(this), frame_(frame) {
  DCHECK(receiver);
  DCHECK(frame_);

  receiver.Bind()->SetApplicationController(binding_.NewBinding());

  binding_.set_error_handler([](zx_status_t status) {
    if (status != ZX_ERR_PEER_CLOSED && status != ZX_ERR_CANCELED) {
      ZX_LOG(WARNING, status) << "Application bindings connection dropped.";
    }
  });
}

ApplicationControllerImpl::~ApplicationControllerImpl() = default;

void ApplicationControllerImpl::SetTouchInputEnabled(bool enable) {
  frame_->SetEnableInput(enable);
}
