// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/mock_virtual_keyboard.h"

#include <lib/async/default.h>

#include "base/run_loop.h"

namespace virtualkeyboard = fuchsia_input_virtualkeyboard;

MockVirtualKeyboardController::MockVirtualKeyboardController() = default;
MockVirtualKeyboardController::~MockVirtualKeyboardController() {
  if (watch_visibility_completer_) {
    watch_visibility_completer_->Close(ZX_ERR_PEER_CLOSED);
  }
  if (binding_) {
    binding_->Close(ZX_ERR_PEER_CLOSED);
  }
}

void MockVirtualKeyboardController::Bind(
    fuchsia_ui_views::ViewRef view_ref,
    virtualkeyboard::TextType text_type,
    fidl::ServerEnd<fuchsia_input_virtualkeyboard::Controller>
        controller_server_end) {
  text_type_ = text_type;
  view_ref_ = std::move(view_ref);
  binding_.emplace(async_get_default_dispatcher(),
                   std::move(controller_server_end), this,
                   base::FidlBindingClosureWarningLogger(
                       "fuchsia.input.virtualkeyboard.Controller"));
}

void MockVirtualKeyboardController::AwaitWatchAndRespondWith(bool is_visible) {
  if (!watch_visibility_completer_) {
    base::RunLoop run_loop;
    on_watch_visibility_ = run_loop.QuitClosure();
    run_loop.Run();
    ASSERT_TRUE(watch_visibility_completer_);
  }

  watch_visibility_completer_->Reply({{.is_visible = is_visible}});
  watch_visibility_completer_.reset();
}

void MockVirtualKeyboardController::WatchVisibility(
    MockVirtualKeyboardController::WatchVisibilityCompleter::Sync& completer) {
  watch_visibility_completer_ = completer.ToAsync();

  if (on_watch_visibility_)
    std::move(on_watch_visibility_).Run();
}

MockVirtualKeyboardControllerCreator::MockVirtualKeyboardControllerCreator(
    base::TestComponentContextForProcess* component_context)
    : binding_(component_context->additional_services(), this) {}

MockVirtualKeyboardControllerCreator::~MockVirtualKeyboardControllerCreator() {
  CHECK(!pending_controller_);
}

std::unique_ptr<MockVirtualKeyboardController>
MockVirtualKeyboardControllerCreator::CreateController() {
  DCHECK(!pending_controller_);

  auto controller = std::make_unique<MockVirtualKeyboardController>();
  pending_controller_ = controller.get();
  return controller;
}

void MockVirtualKeyboardControllerCreator::Create(
    MockVirtualKeyboardControllerCreator::CreateRequest& request,
    MockVirtualKeyboardControllerCreator::CreateCompleter::Sync& completer) {
  CHECK(pending_controller_);
  pending_controller_->Bind(std::move(request.view_ref()), request.text_type(),
                            std::move(request.controller_request()));
  pending_controller_ = nullptr;
}
