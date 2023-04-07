// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/application_controller_impl.h"

#include <fidl/fuchsia.media.sessions2/cpp/hlcpp_conversion.h>
#include <lib/async/default.h>

#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/string_piece.h"

ApplicationControllerImpl::ApplicationControllerImpl(
    fuchsia::web::Frame* frame,
    fidl::Client<chromium_cast::ApplicationContext>& context)
    : frame_(frame) {
  DCHECK(context);
  DCHECK(frame_);

  auto application_controller_endpoints =
      fidl::CreateEndpoints<chromium_cast::ApplicationController>();
  ZX_CHECK(application_controller_endpoints.is_ok(),
           application_controller_endpoints.status_value());
  binding_.emplace(async_get_default_dispatcher(),
                   std::move(application_controller_endpoints->server), this,
                   [](fidl::UnbindInfo info) {
                     LOG_IF(WARNING, info.status() != ZX_ERR_PEER_CLOSED &&
                                         info.status() != ZX_ERR_CANCELED)
                         << "Unbound from chromium.cast.ApplicationController: "
                         << info;
                   });
  auto result = context->SetApplicationController(
      {{.controller = std::move(application_controller_endpoints->client)}});
  LOG_IF(ERROR, result.is_error())
      << base::FidlMethodResultErrorMessage(result, "SetApplicationController");
}

ApplicationControllerImpl::~ApplicationControllerImpl() = default;

void ApplicationControllerImpl::SetTouchInputEnabled(
    ApplicationControllerImpl::SetTouchInputEnabledRequest& request,
    ApplicationControllerImpl::SetTouchInputEnabledCompleter::Sync& completer) {
  auto allow_input_state = request.enable()
                               ? fuchsia::web::AllowInputState::ALLOW
                               : fuchsia::web::AllowInputState::DENY;
  frame_->ConfigureInputTypes(fuchsia::web::InputTypes::GESTURE_TAP |
                                  fuchsia::web::InputTypes::GESTURE_DRAG,
                              allow_input_state);
}

void ApplicationControllerImpl::GetMediaPlayer(
    ApplicationControllerImpl::GetMediaPlayerRequest& request,
    ApplicationControllerImpl::GetMediaPlayerCompleter::Sync& completer) {
  frame_->GetMediaPlayer(fidl::NaturalToHLCPP(request.request()));
}

void ApplicationControllerImpl::SetBlockMediaLoading(
    ApplicationControllerImpl::SetBlockMediaLoadingRequest& request,
    ApplicationControllerImpl::SetBlockMediaLoadingCompleter::Sync& completer) {
  frame_->SetBlockMediaLoading(request.blocked());
}

void ApplicationControllerImpl::GetPrivateMemorySize(
    ApplicationControllerImpl::GetPrivateMemorySizeCompleter::Sync& completer) {
  frame_->GetPrivateMemorySize(
      [completer = completer.ToAsync()](uint64_t private_memory_size) mutable {
        completer.Reply(private_memory_size);
      });
}
