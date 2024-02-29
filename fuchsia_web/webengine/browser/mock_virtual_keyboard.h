// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_MOCK_VIRTUAL_KEYBOARD_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_MOCK_VIRTUAL_KEYBOARD_H_

#include <fidl/fuchsia.input.virtualkeyboard/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include <optional>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/functional/callback.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockVirtualKeyboardController
    : public fidl::Server<fuchsia_input_virtualkeyboard::Controller> {
 public:
  MockVirtualKeyboardController();
  ~MockVirtualKeyboardController() override;

  MockVirtualKeyboardController(MockVirtualKeyboardController&) = delete;
  MockVirtualKeyboardController operator=(MockVirtualKeyboardController&) =
      delete;

  void Bind(fuchsia_ui_views::ViewRef view_ref,
            fuchsia_input_virtualkeyboard::TextType text_type,
            fidl::ServerEnd<fuchsia_input_virtualkeyboard::Controller>
                controller_server_end);

  // Spins a RunLoop until the client calls WatchVisibility().
  void AwaitWatchAndRespondWith(bool is_visible);

  const fuchsia_ui_views::ViewRef& view_ref() const { return view_ref_; }
  fuchsia_input_virtualkeyboard::TextType text_type() const {
    return text_type_;
  }

  // fuchsia_input_virtualkeyboard::Controller implementation.
  MOCK_METHOD1(RequestShow, void(RequestShowCompleter::Sync&));
  MOCK_METHOD1(RequestHide, void(RequestHideCompleter::Sync&));
  MOCK_METHOD2(SetTextType,
               void(SetTextTypeRequest&, SetTextTypeCompleter::Sync&));

 private:
  // fuchsia_input_virtualkeyboard::Controller implementation.
  void WatchVisibility(WatchVisibilityCompleter::Sync& completer) final;

  base::OnceClosure on_watch_visibility_;
  std::optional<fidl::Server<fuchsia_input_virtualkeyboard::Controller>::
                    WatchVisibilityCompleter::Async>
      watch_visibility_completer_;
  fuchsia_ui_views::ViewRef view_ref_;
  fuchsia_input_virtualkeyboard::TextType text_type_;
  std::optional<fidl::ServerBinding<fuchsia_input_virtualkeyboard::Controller>>
      binding_;
};

// Services connection requests for MockVirtualKeyboardControllers.
class MockVirtualKeyboardControllerCreator
    : public fidl::Server<fuchsia_input_virtualkeyboard::ControllerCreator> {
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
  // fuchsia_input_virtualkeyboard implementation.
  void Create(CreateRequest& request, CreateCompleter::Sync& completer) final;

  MockVirtualKeyboardController* pending_controller_ = nullptr;
  base::ScopedNaturalServiceBinding<
      fuchsia_input_virtualkeyboard::ControllerCreator>
      binding_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_MOCK_VIRTUAL_KEYBOARD_H_
