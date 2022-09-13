// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/application_controller_impl.h"

#include <fuchsia/diagnostics/cpp/fidl.h>

#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"

ApplicationControllerImpl::ApplicationControllerImpl(
    fuchsia::web::Frame* frame,
    chromium::cast::ApplicationContext* context)
    : binding_(this), frame_(frame) {
  DCHECK(context);
  DCHECK(frame_);

  context->SetApplicationController(binding_.NewBinding());

  binding_.set_error_handler([](zx_status_t status) {
    if (status != ZX_ERR_PEER_CLOSED && status != ZX_ERR_CANCELED) {
      ZX_LOG(WARNING, status) << "Application bindings connection dropped.";
    }
  });
}

ApplicationControllerImpl::~ApplicationControllerImpl() = default;

void ApplicationControllerImpl::SetTouchInputEnabled(bool enable) {
  frame_->ConfigureInputTypes(fuchsia::web::InputTypes::GESTURE_TAP |
                                  fuchsia::web::InputTypes::GESTURE_DRAG,
                              (enable ? fuchsia::web::AllowInputState::ALLOW
                                      : fuchsia::web::AllowInputState::DENY));
}

void ApplicationControllerImpl::GetMediaPlayer(
    fidl::InterfaceRequest<fuchsia::media::sessions2::Player> request) {
  frame_->GetMediaPlayer(std::move(request));
}

void ApplicationControllerImpl::SetBlockMediaLoading(bool blocked) {
  frame_->SetBlockMediaLoading(blocked);
}

void ApplicationControllerImpl::GetPrivateMemorySize(
    GetPrivateMemorySizeCallback callback) {
  frame_->GetPrivateMemorySize(std::move(callback));
}
