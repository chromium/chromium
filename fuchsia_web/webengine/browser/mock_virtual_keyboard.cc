// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/mock_virtual_keyboard.h"

#include "base/run_loop.h"

namespace virtualkeyboard = fuchsia::input::virtualkeyboard;

MockVirtualKeyboardController::MockVirtualKeyboardController()
    : binding_(this) {}

MockVirtualKeyboardController::~MockVirtualKeyboardController() = default;

void MockVirtualKeyboardController::Bind(
    fuchsia::ui::views::ViewRef view_ref,
    virtualkeyboard::TextType text_type,
    fidl::InterfaceRequest<fuchsia::input::virtualkeyboard::Controller>
        controller_request) {
  text_type_ = text_type;
  view_ref_ = std::move(view_ref);
  binding_.Bind(std::move(controller_request));
}

void MockVirtualKeyboardController::AwaitWatchAndRespondWith(bool is_visible) {
  if (!watch_vis_callback_) {
    base::RunLoop run_loop;
    on_watch_visibility_ = run_loop.QuitClosure();
    run_loop.Run();
    ASSERT_TRUE(watch_vis_callback_);
  }

  (*watch_vis_callback_)(is_visible);
  watch_vis_callback_ = {};
}

void MockVirtualKeyboardController::WatchVisibility(
    virtualkeyboard::Controller::WatchVisibilityCallback callback) {
  watch_vis_callback_ = std::move(callback);

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
    fuchsia::ui::views::ViewRef view_ref,
    fuchsia::input::virtualkeyboard::TextType text_type,
    fidl::InterfaceRequest<fuchsia::input::virtualkeyboard::Controller>
        controller_request) {
  CHECK(pending_controller_);
  pending_controller_->Bind(std::move(view_ref), text_type,
                            std::move(controller_request));
  pending_controller_ = nullptr;
}
