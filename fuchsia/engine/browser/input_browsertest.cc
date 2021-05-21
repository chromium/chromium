// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/ui/input3/cpp/fidl.h>
#include <fuchsia/ui/input3/cpp/fidl_test_base.h>
#include <memory>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/test/frame_for_test.h"
#include "fuchsia/engine/test/scenic_test_helper.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using fuchsia::input::Key;
using fuchsia::ui::input3::KeyEvent;
using fuchsia::ui::input3::KeyEventType;

namespace {

const char kKeyDown[] = "keydown";
const char kKeyPress[] = "keypress";
const char kKeyUp[] = "keyup";
const char kKeyDicts[] = "keyDicts";

KeyEvent FakeKeyEvent(Key key, KeyEventType event_type) {
  KeyEvent key_event;
  key_event.set_timestamp(base::TimeTicks::Now().ToZxTime());
  key_event.set_type(event_type);
  key_event.set_key(key);
  return key_event;
}

std::unique_ptr<base::Value> ExpectedKeyValue(base::StringPiece code,
                                              base::StringPiece key,
                                              base::StringPiece type) {
  std::unique_ptr<base::Value> expected =
      std::make_unique<base::DictionaryValue>();
  expected->SetStringKey("code", code);
  expected->SetStringKey("key", key);
  expected->SetStringKey("type", type);
  return expected;
}

class FakeKeyboard : public fuchsia::ui::input3::testing::Keyboard_TestBase {
 public:
  explicit FakeKeyboard(sys::OutgoingDirectory* additional_services) {
    keyboard_binding_.emplace(additional_services, this);
  }
  ~FakeKeyboard() override = default;

  FakeKeyboard(const FakeKeyboard&) = delete;
  FakeKeyboard& operator=(const FakeKeyboard&) = delete;

  // Sends |key_event| to |listener_|;
  void SendKeyEvent(KeyEvent key_event) {
    listener_->OnKeyEvent(std::move(key_event),
                          [num_sent_events = num_sent_events_,
                           this](fuchsia::ui::input3::KeyEventStatus status) {
                            ASSERT_EQ(num_acked_events_, num_sent_events)
                                << "Key events are acked out of order";
                            num_acked_events_++;
                          });
    num_sent_events_++;
  }

  // fuchsia::ui::input3::Keyboard implementation.
  void AddListener(
      fuchsia::ui::views::ViewRef view_ref,
      fidl::InterfaceHandle<::fuchsia::ui::input3::KeyboardListener> listener,
      AddListenerCallback callback) final {
    // This implementation is only set up to have up to one listener.
    DCHECK(!listener_);
    listener_ = listener.Bind();
    callback();
  }

  void NotImplemented_(const std::string& name) final {
    NOTIMPLEMENTED() << name;
  }

 private:
  fuchsia::ui::input3::KeyboardListenerPtr listener_;
  absl::optional<base::ScopedServiceBinding<fuchsia::ui::input3::Keyboard>>
      keyboard_binding_;

  // Counters to make sure key events are acked in order.
  int num_sent_events_ = 0;
  int num_acked_events_ = 0;
};

class InputTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  InputTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~InputTest() override = default;

  InputTest(const InputTest&) = delete;
  InputTest& operator=(const InputTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    fuchsia::web::CreateFrameParams params;
    frame_for_test_ =
        cr_fuchsia::FrameForTest::Create(context(), std::move(params));

    // Set up services needed for the test. The keyboard service is included in
    // the allowed services by default. The real service needs to be removed so
    // it can be replaced by this fake implementation.
    component_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    component_context_->additional_services()
        ->RemovePublicService<fuchsia::ui::input3::Keyboard>();
    keyboard_service_.emplace(component_context_->additional_services());

    fuchsia::web::NavigationControllerPtr controller;
    frame_for_test_.ptr()->GetNavigationController(controller.NewRequest());
    const GURL test_url(embedded_test_server()->GetURL("/keyevents.html"));
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
    frame_for_test_.navigation_listener().RunUntilUrlEquals(test_url);

    fuchsia::web::FramePtr* frame_ptr = &(frame_for_test_.ptr());
    scenic_test_helper_.CreateScenicView(
        context_impl()->GetFrameImplForTest(frame_ptr), frame_for_test_.ptr());
    scenic_test_helper_.SetUpViewForInteraction(
        context_impl()->GetFrameImplForTest(frame_ptr)->web_contents());
  }

  // Used to publish fake services.
  absl::optional<base::TestComponentContextForProcess> component_context_;

  cr_fuchsia::FrameForTest frame_for_test_;
  cr_fuchsia::ScenicTestHelper scenic_test_helper_;
  absl::optional<FakeKeyboard> keyboard_service_;
};

// Check that regular character keys are sent and received correctly.
IN_PROC_BROWSER_TEST_F(InputTest, CharacterKeys) {
  const int kExpectedCharacterEventCount = 6;

  // Send key press events from the Fuchsia keyboard service.
  // Pressing character keys will generate a JavaScript keydown event followed
  // by a keypress event. Releasing any key generates a keyup event.
  keyboard_service_->SendKeyEvent(FakeKeyEvent(Key::A, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::KEY_8, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::KEY_8, KeyEventType::RELEASED));
  keyboard_service_->SendKeyEvent(FakeKeyEvent(Key::A, KeyEventType::RELEASED));
  frame_for_test_.navigation_listener().RunUntilTitleEquals(
      base::NumberToString(kExpectedCharacterEventCount));

  absl::optional<base::Value> result =
      cr_fuchsia::ExecuteJavaScript(frame_for_test_.ptr().get(), kKeyDicts);

  base::ListValue expected;
  expected.Set(0, ExpectedKeyValue("KeyA", "a", kKeyDown));
  expected.Set(1, ExpectedKeyValue("KeyA", "a", kKeyPress));
  expected.Set(2, ExpectedKeyValue("Digit8", "8", kKeyDown));
  expected.Set(3, ExpectedKeyValue("Digit8", "8", kKeyPress));
  expected.Set(4, ExpectedKeyValue("Digit8", "8", kKeyUp));
  expected.Set(5, ExpectedKeyValue("KeyA", "a", kKeyUp));

  EXPECT_EQ(*result, expected);
}

IN_PROC_BROWSER_TEST_F(InputTest, ShiftCharacterKeys) {
  const int kExpectedShiftCharacterEventCount = 10;
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::LEFT_SHIFT, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(FakeKeyEvent(Key::B, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::KEY_3, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::SPACE, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::LEFT_SHIFT, KeyEventType::RELEASED));
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::DOT, KeyEventType::PRESSED));
  frame_for_test_.navigation_listener().RunUntilTitleEquals(
      base::NumberToString(kExpectedShiftCharacterEventCount));

  // Note that non-character keys (e.g. shift, control) only generate key down
  // and key up web events. They do not generate key pressed events.
  absl::optional<base::Value> result =
      cr_fuchsia::ExecuteJavaScript(frame_for_test_.ptr().get(), kKeyDicts);

  base::ListValue expected;
  expected.Set(0, ExpectedKeyValue("ShiftLeft", "Shift", kKeyDown));
  expected.Set(1, ExpectedKeyValue("KeyB", "B", kKeyDown));
  expected.Set(2, ExpectedKeyValue("KeyB", "B", kKeyPress));
  expected.Set(3, ExpectedKeyValue("Digit3", "#", kKeyDown));
  expected.Set(4, ExpectedKeyValue("Digit3", "#", kKeyPress));
  expected.Set(5, ExpectedKeyValue("Space", " ", kKeyDown));
  expected.Set(6, ExpectedKeyValue("Space", " ", kKeyPress));
  expected.Set(7, ExpectedKeyValue("ShiftLeft", "Shift", kKeyUp));
  expected.Set(8, ExpectedKeyValue("Period", ".", kKeyDown));
  expected.Set(9, ExpectedKeyValue("Period", ".", kKeyPress));

  EXPECT_EQ(*result, expected);
}

IN_PROC_BROWSER_TEST_F(InputTest, ShiftNonCharacterKeys) {
  const int kExpectedShiftNonCharacterEventCount = 5;

  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::RIGHT_SHIFT, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::ENTER, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::LEFT_CTRL, KeyEventType::PRESSED));
  keyboard_service_->SendKeyEvent(
      FakeKeyEvent(Key::RIGHT_SHIFT, KeyEventType::RELEASED));
  frame_for_test_.navigation_listener().RunUntilTitleEquals(
      base::NumberToString(kExpectedShiftNonCharacterEventCount));

  // Note that non-character keys (e.g. shift, control) only generate key down
  // and key up web events. They do not generate key pressed events.
  absl::optional<base::Value> result =
      cr_fuchsia::ExecuteJavaScript(frame_for_test_.ptr().get(), kKeyDicts);

  base::ListValue expected;
  expected.Set(0, ExpectedKeyValue("ShiftRight", "Shift", kKeyDown));
  expected.Set(1, ExpectedKeyValue("Enter", "Enter", kKeyDown));
  expected.Set(2, ExpectedKeyValue("Enter", "Enter", kKeyPress));
  expected.Set(3, ExpectedKeyValue("ControlLeft", "Control", kKeyDown));
  expected.Set(4, ExpectedKeyValue("ShiftRight", "Shift", kKeyUp));

  EXPECT_EQ(*result, expected);
}

IN_PROC_BROWSER_TEST_F(InputTest, Disconnect) {
  // Disconnect the keyboard service.
  keyboard_service_.reset();

  frame_for_test_.navigation_listener().RunUntilTitleEquals("loaded");

  // Make sure the page is still available and there are no crashes.
  EXPECT_TRUE(cr_fuchsia::ExecuteJavaScript(frame_for_test_.ptr().get(), "true")
                  ->GetBool());
}

}  // namespace