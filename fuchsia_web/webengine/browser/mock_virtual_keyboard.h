// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_MOCK_VIRTUAL_KEYBOARD_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_MOCK_VIRTUAL_KEYBOARD_H_

#include <fuchsia/input/virtualkeyboard/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/functional/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class MockVirtualKeyboardController
    : public fuchsia::input::virtualkeyboard::Controller {
 public:
  MockVirtualKeyboardController();
  ~MockVirtualKeyboardController() override;

  MockVirtualKeyboardController(MockVirtualKeyboardController&) = delete;
  MockVirtualKeyboardController operator=(MockVirtualKeyboardController&) =
      delete;

  void Bind(fuchsia::ui::views::ViewRef view_ref,
            fuchsia::input::virtualkeyboard::TextType text_type,
            fidl::InterfaceRequest<fuchsia::input::virtualkeyboard::Controller>
                controller_request);

  // Spins a RunLoop until the client calls WatchVisibility().
  void AwaitWatchAndRespondWith(bool is_visible);

  const fuchsia::ui::views::ViewRef& view_ref() const { return view_ref_; }
  fuchsia::input::virtualkeyboard::TextType text_type() const {
    return text_type_;
  }

  // fuchsia::input::virtualkeyboard::Controller implementation.
  MOCK_METHOD0(RequestShow, void());
  MOCK_METHOD0(RequestHide, void());
  MOCK_METHOD1(SetTextType,
               void(fuchsia::input::virtualkeyboard::TextType text_type));

 private:
  // fuchsia::input::virtualkeyboard::Controller implementation.
  void WatchVisibility(
      fuchsia::input::virtualkeyboard::Controller::WatchVisibilityCallback
          callback) final;

  base::OnceClosure on_watch_visibility_;
  absl::optional<
      fuchsia::input::virtualkeyboard::Controller::WatchVisibilityCallback>
      watch_vis_callback_;
  fuchsia::ui::views::ViewRef view_ref_;
  fuchsia::input::virtualkeyboard::TextType text_type_;
  fidl::Binding<fuchsia::input::virtualkeyboard::Controller> binding_;
};

// Services connection requests for MockVirtualKeyboardControllers.
class MockVirtualKeyboardControllerCreator
    : public fuchsia::input::virtualkeyboard::ControllerCreator {
 public:
  explicit MockVirtualKeyboardControllerCreator(
      base::TestComponentContextForProcess* component_context);
  ~MockVirtualKeyboardControllerCreator() override;

  MockVirtualKeyboardControllerCreator(MockVirtualKeyboardControllerCreator&) =
      delete;
  MockVirtualKeyboardControllerCreator operator=(
      MockVirtualKeyboardControllerCreator&) = delete;

  // Returns an unbound MockVirtualKeyboardController, which will later be
  // connected when |this| handles a call to the FIDL method Create().
  // The MockVirtualKeyboardController must not be destroyed before |this|.
  std::unique_ptr<MockVirtualKeyboardController> CreateController();

 private:
  // fuchsia::input::virtualkeyboard implementation.
  void Create(
      fuchsia::ui::views::ViewRef view_ref,
      fuchsia::input::virtualkeyboard::TextType text_type,
      fidl::InterfaceRequest<fuchsia::input::virtualkeyboard::Controller>
          controller_request) final;

  MockVirtualKeyboardController* pending_controller_ = nullptr;
  base::ScopedServiceBinding<fuchsia::input::virtualkeyboard::ControllerCreator>
      binding_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_MOCK_VIRTUAL_KEYBOARD_H_
