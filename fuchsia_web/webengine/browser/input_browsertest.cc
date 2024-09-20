// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fidl/fuchsia.input.virtualkeyboard/cpp/wire_messaging.h>
#include <fidl/fuchsia.ui.input3/cpp/fidl.h>
#include <lib/async/default.h>

#include <memory>
#include <string_view>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/features.h"
#include "fuchsia_web/webengine/test/scenic_test_helper.h"
#include "fuchsia_web/webengine/test/scoped_connection_checker.h"
#include "fuchsia_web/webengine/test/test_data.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/public/ozone_platform.h"

using fuchsia_input::Key;
using fuchsia_ui_input3::KeyEvent;
using fuchsia_ui_input3::KeyEventType;

namespace {

const char kKeyDown[] = "keydown";
const char kKeyPress[] = "keypress";
const char kKeyUp[] = "keyup";
const char kKeyDicts[] = "keyDicts";

// Returns a KeyEvent with |key_meaning| set based on the supplied codepoint,
// the |key| field left not set.
KeyEvent CreateCharacterKeyEvent(uint32_t codepoint, KeyEventType event_type) {
  return {{
      .timestamp = base::TimeTicks::Now().ToZxTime(),
      .type = event_type,
      .key_meaning = fuchsia_ui_input3::KeyMeaning::WithCodepoint(codepoint),
  }};
}

struct KeyEventOptions {
  bool repeat;
  std::vector<fuchsia_ui_input3::Modifiers> modifiers;
};

// Returns a KeyEvent with both |key| and |key_meaning| set.
KeyEvent CreateKeyEvent(Key key,
                        fuchsia_ui_input3::KeyMeaning key_meaning,
                        KeyEventType event_type,
                        KeyEventOptions options = {}) {
  KeyEvent key_event{{
      .timestamp = base::TimeTicks::Now().ToZxTime(),
      .type = event_type,
      .key = key,
      .key_meaning = std::move(key_meaning),
  }};

  if (options.repeat) {
    // Chromium doesn't look at the value of this, it just check if the field is
    // present.
    key_event.repeat_sequence(1);
  }
  if (!options.modifiers.empty()) {
    fuchsia_ui_input3::Modifiers modifiers;
    for (const auto modifier : options.modifiers) {
      modifiers |= modifier;
    }
    key_event.modifiers(modifiers);
  }
  return key_event;
}
KeyEvent CreateKeyEvent(Key key,
                        uint32_t codepoint,
                        KeyEventType event_type,
                        KeyEventOptions options = {}) {
  return CreateKeyEvent(
      key, fuchsia_ui_input3::KeyMeaning::WithCodepoint(std::move(codepoint)),
      event_type, options);
}
KeyEvent CreateKeyEvent(Key key,
                        fuchsia_ui_input3::NonPrintableKey non_printable_key,
                        KeyEventType event_type,
                        KeyEventOptions options = {}) {
  return CreateKeyEvent(key,
                        fuchsia_ui_input3::KeyMeaning::WithNonPrintableKey(
                            std::move(non_printable_key)),
                        event_type, options);
}

base::Value::List FuchsiaModifiersToWebModifiers(
    const std::vector<fuchsia_ui_input3::Modifiers> fuchsia_modifiers) {
  base::Value::List web_modifiers;
  for (const auto modifier : fuchsia_modifiers) {
    if (modifier == fuchsia_ui_input3::Modifiers::kAlt) {
      web_modifiers.Append("Alt");
    } else if (modifier == fuchsia_ui_input3::Modifiers::kAltGraph) {
      web_modifiers.Append("AltGraph");
    } else if (modifier == fuchsia_ui_input3::Modifiers::kCapsLock) {
      web_modifiers.Append("CapsLock");
    } else if (modifier == fuchsia_ui_input3::Modifiers::kCtrl) {
      web_modifiers.Append("Control");
    } else if (modifier == fuchsia_ui_input3::Modifiers::kMeta) {
      web_modifiers.Append("Meta");
    } else if (modifier == fuchsia_ui_input3::Modifiers::kNumLock) {
      web_modifiers.Append("NumLock");
    } else if (modifier == fuchsia_ui_input3::Modifiers::kScrollLock) {
      web_modifiers.Append("ScrollLock");
    } else if (modifier == fuchsia_ui_input3::Modifiers::kShift) {
      web_modifiers.Append("Shift");
    } else {
      NOTREACHED() << static_cast<uint64_t>(modifier) << " has no web mapping";
    }
  }
  return web_modifiers;
}

base::Value ExpectedKeyValue(std::string_view code,
                             std::string_view key,
                             std::string_view type,
                             KeyEventOptions options = {}) {
  base::Value::Dict expected;
  expected.Set("code", code);
  expected.Set("key", key);
  expected.Set("type", type);
  expected.Set("repeat", options.repeat);
  expected.Set("modifiers", FuchsiaModifiersToWebModifiers(options.modifiers));
  return base::Value(std::move(expected));
}

class FakeKeyboard : public fidl::Server<fuchsia_ui_input3::Keyboard> {
 public:
  explicit FakeKeyboard(sys::OutgoingDirectory* additional_services)
      : binding_(additional_services, this) {}
  ~FakeKeyboard() override = default;

  FakeKeyboard(const FakeKeyboard&) = delete;
  FakeKeyboard& operator=(const FakeKeyboard&) = delete;

  base::ScopedNaturalServiceBinding<fuchsia_ui_input3::Keyboard>* binding() {
    return &binding_;
  }

  // Sends |key_event| to |listener_|;
  void SendKeyEvent(KeyEvent key_event) {
    listener_->OnKeyEvent(std::move(key_event))
        .Then([num_sent_events = num_sent_events_,
               this](const fidl::Result<
                     fuchsia_ui_input3::KeyboardListener::OnKeyEvent>& result) {
          ASSERT_EQ(num_acked_events_, num_sent_events)
              << "Key events are acked out of order";
          num_acked_events_++;
        });
    num_sent_events_++;
  }

  // fuchsia_ui_input3::Keyboard implementation.
  void AddListener(AddListenerRequest& request,
                   AddListenerCompleter::Sync& completer) final {
    // This implementation is only set up to have up to one listener.
    EXPECT_FALSE(listener_);
    listener_.Bind(std::move(request.listener()),
                   async_get_default_dispatcher());
    completer.Reply();
  }

 private:
  fidl::Client<fuchsia_ui_input3::KeyboardListener> listener_;
  base::ScopedNaturalServiceBinding<fuchsia_ui_input3::Keyboard> binding_;

  // Counters to make sure key events are acked in order.
  int num_sent_events_ = 0;
  int num_acked_events_ = 0;
};

class KeyboardInputTest : public WebEngineBrowserTest {
 public:
  KeyboardInputTest() { set_test_server_root(base::FilePath(kTestServerRoot)); }
  ~KeyboardInputTest() override = default;

  KeyboardInputTest(const KeyboardInputTest&) = delete;
  KeyboardInputTest& operator=(const KeyboardInputTest&) = delete;

 protected:
  virtual void SetUpService() {
    keyboard_service_.emplace(component_context_->additional_services());
  }

  void SetUp() override {
    if (ui::OzonePlatform::GetPlatformNameForTest() == "headless") {
      GTEST_SKIP() << "Keyboard inputs are ignored in headless mode.";
    }

    scoped_feature_list_.InitWithFeatures({features::kKeyboardInput}, {});
    WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WebEngineBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    fuchsia::web::CreateFrameParams params;
    frame_for_test_ = FrameForTest::Create(context(), std::move(params));

    // Set up services needed for the test. The keyboard service is included in
    // the allowed services by default. The real service needs to be removed so
    // it can be replaced by this fake implementation.
    component_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    component_context_->additional_services()
        ->RemovePublicService<fuchsia_ui_input3::Keyboard>(
            fidl::DiscoverableProtocolName<fuchsia_ui_input3::Keyboard>);
    SetUpService();
    virtual_keyboard_checker_.emplace(
        component_context_->additional_services());

    fuchsia::web::NavigationControllerPtr controller;
    frame_for_test_.ptr()->GetNavigationController(controller.NewRequest());
    const GURL test_url(embedded_test_server()->GetURL("/keyevents.html"));
    EXPECT_TRUE(LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
    frame_for_test_.navigation_listener().RunUntilUrlEquals(test_url);

    fuchsia::web::FramePtr* frame_ptr = &(frame_for_test_.ptr());
    scenic_test_helper_.CreateScenicView(
        context_impl()->GetFrameImplForTest(frame_ptr), frame_for_test_.ptr());
    scenic_test_helper_.SetUpViewForInteraction(
        context_impl()->GetFrameImplForTest(frame_ptr)->web_contents());
  }

  void TearDownOnMainThread() override {
    frame_for_test_ = {};
    WebEngineBrowserTest::TearDownOnMainThread();
  }

  // The tests expect to have input processed immediately, even if the
  // content has not been displayed yet. That's fine for the test, but
  // we need to explicitly allow it.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("allow-pre-commit-input");
  }

  void ExpectKeyEventsEqual(base::Value::List expected) {
    frame_for_test_.navigation_listener().RunUntilTitleEquals(
        base::NumberToString(expected.size()));

    std::optional<base::Value> actual =
        ExecuteJavaScript(frame_for_test_.ptr().get(), kKeyDicts);
    EXPECT_EQ(*actual, base::Value(std::move(expected)));
  }

  template <typename... Args>
  void ExpectKeyEventsEqual(Args... events) {
    base::Value::List expected =
        content::ListValueOf(std::forward<Args>(events)...).TakeList();
    ExpectKeyEventsEqual(std::move(expected));
  }

  // Used to publish fake services.
  std::optional<base::TestComponentContextForProcess> component_context_;

  FrameForTest frame_for_test_;
  ScenicTestHelper scenic_test_helper_;
  std::optional<FakeKeyboard> keyboard_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<
      NeverConnectedChecker<fuchsia_input_virtualkeyboard::ControllerCreator>>
      virtual_keyboard_checker_;
};

// Check that printable keys are sent and received correctly.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, PrintableKeys) {
  // Send key press events from the Fuchsia keyboard service.
  // Pressing character keys will generate a JavaScript keydown event followed
  // by a keypress event. Releasing any key generates a keyup event.
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kA, 'a', KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kKey8, '8', KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kKey8, '8', KeyEventType::kReleased));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kA, 'a', KeyEventType::kReleased));

  ExpectKeyEventsEqual(ExpectedKeyValue("KeyA", "a", kKeyDown),
                       ExpectedKeyValue("KeyA", "a", kKeyPress),
                       ExpectedKeyValue("Digit8", "8", kKeyDown),
                       ExpectedKeyValue("Digit8", "8", kKeyPress),
                       ExpectedKeyValue("Digit8", "8", kKeyUp),
                       ExpectedKeyValue("KeyA", "a", kKeyUp));
}

// Check that character virtual keys are sent and received correctly.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, Characters) {
  // Send key press events from the Fuchsia keyboard service.
  // Pressing character keys will generate a JavaScript keydown event followed
  // by a keypress event. Releasing any key generates a keyup event.
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent('A', KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent('A', KeyEventType::kReleased));
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent('b', KeyEventType::kPressed));

  ExpectKeyEventsEqual(
      ExpectedKeyValue("", "A", kKeyDown), ExpectedKeyValue("", "A", kKeyPress),
      ExpectedKeyValue("", "A", kKeyUp), ExpectedKeyValue("", "b", kKeyDown),
      ExpectedKeyValue("", "b", kKeyPress));
}

// Verify that character events are not affected by active modifiers.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, ShiftCharacter) {
  // TODO(fxbug.dev/106600): Update the WithCodepoint(0)s when the platform is
  // fixed to provide valid KeyMeanings for these keys.
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kLeftShift, 0, KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent('a', KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent('a', KeyEventType::kReleased));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kLeftShift, 0, KeyEventType::kReleased));

  ExpectKeyEventsEqual(
      ExpectedKeyValue("ShiftLeft", "Shift", kKeyDown),
      ExpectedKeyValue("", "a", kKeyDown),   // Remains lowercase.
      ExpectedKeyValue("", "a", kKeyPress),  // You guessed it! Still lowercase.
      ExpectedKeyValue("", "a", kKeyUp),     // Wow, lowercase just won't quit.
      ExpectedKeyValue("ShiftLeft", "Shift", kKeyUp));
}

// Verifies that codepoints outside the 16-bit Unicode BMP are rejected.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, CharacterInBmp) {
  const wchar_t kSigma = 0x03C3;
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent(kSigma, KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent(kSigma, KeyEventType::kReleased));

  std::string expected_utf8;
  ASSERT_TRUE(base::WideToUTF8(&kSigma, 1, &expected_utf8));
  ExpectKeyEventsEqual(ExpectedKeyValue("", expected_utf8, kKeyDown),
                       ExpectedKeyValue("", expected_utf8, kKeyPress),
                       ExpectedKeyValue("", expected_utf8, kKeyUp));
}

// Verifies that codepoints beyond the range of allowable UCS-2 values
// are rejected.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, CharacterBeyondBmp) {
  const uint32_t kRamenEmoji = 0x1F35C;

  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent(kRamenEmoji, KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent(kRamenEmoji, KeyEventType::kReleased));
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent('a', KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateCharacterKeyEvent('a', KeyEventType::kReleased));

  ExpectKeyEventsEqual(ExpectedKeyValue("", "a", kKeyDown),
                       ExpectedKeyValue("", "a", kKeyPress),
                       ExpectedKeyValue("", "a", kKeyUp));
}

IN_PROC_BROWSER_TEST_F(KeyboardInputTest, ShiftPrintableKeys) {
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kLeftShift, 0, KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kB, 'B', KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kKey1, '!', KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kSpace, ' ', KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kLeftShift, 0, KeyEventType::kReleased));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kDot, '.', KeyEventType::kPressed));

  // Note that non-character keys (e.g. shift, control) only generate key down
  // and key up web events. They do not generate key pressed events.
  ExpectKeyEventsEqual(ExpectedKeyValue("ShiftLeft", "Shift", kKeyDown),
                       ExpectedKeyValue("KeyB", "B", kKeyDown),
                       ExpectedKeyValue("KeyB", "B", kKeyPress),
                       ExpectedKeyValue("Digit1", "!", kKeyDown),
                       ExpectedKeyValue("Digit1", "!", kKeyPress),
                       ExpectedKeyValue("Space", " ", kKeyDown),
                       ExpectedKeyValue("Space", " ", kKeyPress),
                       ExpectedKeyValue("ShiftLeft", "Shift", kKeyUp),
                       ExpectedKeyValue("Period", ".", kKeyDown),
                       ExpectedKeyValue("Period", ".", kKeyPress));
}

IN_PROC_BROWSER_TEST_F(KeyboardInputTest, ShiftNonPrintableKeys) {
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kRightShift, 0, KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kEnter, fuchsia_ui_input3::NonPrintableKey::kEnter,
                     KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kLeftCtrl, 0, KeyEventType::kPressed));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kRightShift, 0, KeyEventType::kReleased));

  // Note that non-character keys (e.g. shift, control) only generate key down
  // and key up web events. They do not generate key pressed events.
  ExpectKeyEventsEqual(ExpectedKeyValue("ShiftRight", "Shift", kKeyDown),
                       ExpectedKeyValue("Enter", "Enter", kKeyDown),
                       ExpectedKeyValue("Enter", "Enter", kKeyPress),
                       ExpectedKeyValue("ControlLeft", "Control", kKeyDown),
                       ExpectedKeyValue("ShiftRight", "Shift", kKeyUp));
}

IN_PROC_BROWSER_TEST_F(KeyboardInputTest, RepeatedKeys) {
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kA, 'a', KeyEventType::kPressed, {.repeat = true}));
  keyboard_service_->SendKeyEvent(CreateKeyEvent(
      Key::kKey8, '8', KeyEventType::kPressed, {.repeat = true}));

  // Note that non-character keys (e.g. shift, control) only generate key down
  // and key up web events. They do not generate key pressed events.
  ExpectKeyEventsEqual(
      ExpectedKeyValue("KeyA", "a", kKeyDown, {.repeat = true}),
      ExpectedKeyValue("KeyA", "a", kKeyPress, {.repeat = true}),
      ExpectedKeyValue("Digit8", "8", kKeyDown, {.repeat = true}),
      ExpectedKeyValue("Digit8", "8", kKeyPress, {.repeat = true}));
}

IN_PROC_BROWSER_TEST_F(KeyboardInputTest, AllSupportedWebModifierKeys) {
  // All modifiers in the FIDL protocol that Chrome handles on the web.
  //
  // Missing modifiers:
  // * LEFT_*/RIGHT_* are not valid by themselves.
  // * FUNCTION and SYMBOL. See AllUnsupportedWebModifierKeys test.
  const std::vector kAllSupportedModifiers = {
      fuchsia_ui_input3::Modifiers::kCapsLock,
      fuchsia_ui_input3::Modifiers::kNumLock,
      fuchsia_ui_input3::Modifiers::kScrollLock,
      fuchsia_ui_input3::Modifiers::kShift,
      fuchsia_ui_input3::Modifiers::kAlt,
      fuchsia_ui_input3::Modifiers::kAltGraph,
      fuchsia_ui_input3::Modifiers::kMeta,
      fuchsia_ui_input3::Modifiers::kCtrl};
  for (const auto& modifier : kAllSupportedModifiers) {
    keyboard_service_->SendKeyEvent(CreateKeyEvent(
        Key::kM, 'm', KeyEventType::kPressed, {.modifiers = {modifier}}));
  }

  base::Value::List expected_events;
  for (const auto& modifier : kAllSupportedModifiers) {
    expected_events.Append(
        ExpectedKeyValue("KeyM", "m", kKeyDown, {.modifiers = {modifier}}));
    // Chrome doesn't emit keypress events when an ALT or CTRL modifier is
    // present.
    if (modifier != fuchsia_ui_input3::Modifiers::kAlt &&
        modifier != fuchsia_ui_input3::Modifiers::kCtrl) {
      expected_events.Append(
          ExpectedKeyValue("KeyM", "m", kKeyPress, {.modifiers = {modifier}}));
    }
  }

  ExpectKeyEventsEqual(std::move(expected_events));
}

IN_PROC_BROWSER_TEST_F(KeyboardInputTest, AllUnsupportedWebModifierKeys) {
  // All modifiers in the FIDL protocol that Chrome doesn't handle on the web
  // because they aren't included in
  // https://crsrc.org/c/ui/events/blink/blink_event_util.cc;l=268?q=EventFlagsToWebEventModifiers
  const std::vector kAllUnsupportedModifiers = {
      fuchsia_ui_input3::Modifiers::kFunction,
      fuchsia_ui_input3::Modifiers::kSymbol};
  for (const auto& modifier : kAllUnsupportedModifiers) {
    keyboard_service_->SendKeyEvent(CreateKeyEvent(
        Key::kM, 'm', KeyEventType::kPressed, {.modifiers = {modifier}}));
  }

  base::Value::List expected_events;
  for (size_t i = 0; i < kAllUnsupportedModifiers.size(); ++i) {
    expected_events.Append(ExpectedKeyValue("KeyM", "m", kKeyDown, {}));
    expected_events.Append(ExpectedKeyValue("KeyM", "m", kKeyPress, {}));
  }

  ExpectKeyEventsEqual(std::move(expected_events));
}

// This is a spot check to make sure that modifiers work with other keys and in
// combination with each other.
IN_PROC_BROWSER_TEST_F(KeyboardInputTest, AssortedModifierKeyCombos) {
  // Test that sending LEFT/RIGHT SHIFT with agnostic SHIFT passes DCHECK.
  keyboard_service_->SendKeyEvent(CreateKeyEvent(
      Key::kA, 'a', KeyEventType::kPressed,
      {.modifiers = {fuchsia_ui_input3::Modifiers::kShift,
                     fuchsia_ui_input3::Modifiers::kLeftShift,
                     fuchsia_ui_input3::Modifiers::kRightShift}}));
  // Test that sending LEFT/RIGHT ALT with agnostic ALT passes DCHECK.
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kKey8, '8', KeyEventType::kPressed,
                     {.modifiers = {fuchsia_ui_input3::Modifiers::kAlt,
                                    fuchsia_ui_input3::Modifiers::kLeftAlt,
                                    fuchsia_ui_input3::Modifiers::kRightAlt}}));
  // Test that sending LEFT/RIGHT META with agnostic META passes DCHECK.
  keyboard_service_->SendKeyEvent(CreateKeyEvent(
      Key::kB, 'b', KeyEventType::kPressed,
      {.modifiers = {fuchsia_ui_input3::Modifiers::kMeta,
                     fuchsia_ui_input3::Modifiers::kLeftMeta,
                     fuchsia_ui_input3::Modifiers::kRightMeta}}));
  // Test that sending LEFT/RIGHT CTRL with agnostic CTRL passes DCHECK.
  keyboard_service_->SendKeyEvent(CreateKeyEvent(
      Key::kLeft, 0, KeyEventType::kPressed,
      {.modifiers = {fuchsia_ui_input3::Modifiers::kCtrl,
                     fuchsia_ui_input3::Modifiers::kLeftCtrl,
                     fuchsia_ui_input3::Modifiers::kRightCtrl}}));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kP, 'p', KeyEventType::kPressed,
                     {.modifiers = {fuchsia_ui_input3::Modifiers::kCtrl,
                                    fuchsia_ui_input3::Modifiers::kShift}}));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kRight, 0, KeyEventType::kPressed,
                     {.modifiers = {fuchsia_ui_input3::Modifiers::kAltGraph}}));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kUp, 0, KeyEventType::kPressed,
                     {.modifiers = {fuchsia_ui_input3::Modifiers::kCapsLock}}));
  keyboard_service_->SendKeyEvent(
      CreateKeyEvent(Key::kDown, 0, KeyEventType::kPressed,
                     {.modifiers = {fuchsia_ui_input3::Modifiers::kNumLock}}));
  keyboard_service_->SendKeyEvent(CreateKeyEvent(
      Key::kLeft, 0, KeyEventType::kPressed,
      {.modifiers = {fuchsia_ui_input3::Modifiers::kScrollLock}}));

  ExpectKeyEventsEqual(
      ExpectedKeyValue("KeyA", "a", kKeyDown,
                       {.modifiers = {fuchsia_ui_input3::Modifiers::kShift}}),
      ExpectedKeyValue("KeyA", "a", kKeyPress,
                       {.modifiers = {fuchsia_ui_input3::Modifiers::kShift}}),
      ExpectedKeyValue("Digit8", "8", kKeyDown,
                       {.modifiers = {fuchsia_ui_input3::Modifiers::kAlt}}),
      ExpectedKeyValue("KeyB", "b", kKeyDown,
                       {.modifiers = {fuchsia_ui_input3::Modifiers::kMeta}}),
      ExpectedKeyValue("KeyB", "b", kKeyPress,
                       {.modifiers = {fuchsia_ui_input3::Modifiers::kMeta}}),
      ExpectedKeyValue("ArrowLeft", "ArrowLeft", kKeyDown,
                       {.modifiers = {fuchsia_ui_input3::Modifiers::kCtrl}}),
      ExpectedKeyValue("KeyP", "p", kKeyDown,
                       {.modifiers = {fuchsia_ui_input3::Modifiers::kCtrl,
                                      fuchsia_ui_input3::Modifiers::kShift}}),
      ExpectedKeyValue("KeyP", "p", kKeyPress,
                       {.modifiers = {fuchsia_ui_input3::Modifiers::kCtrl,
                                      fuchsia_ui_input3::Modifiers::kShift}}),
      ExpectedKeyValue(
          "ArrowRight", "ArrowRight", kKeyDown,
          {.modifiers = {fuchsia_ui_input3::Modifiers::kAltGraph}}),
      ExpectedKeyValue(
          "ArrowUp", "ArrowUp", kKeyDown,
          {.modifiers = {fuchsia_ui_input3::Modifiers::kCapsLock}}),
      ExpectedKeyValue("ArrowDown", "ArrowDown", kKeyDown,
                       {.modifiers = {fuchsia_ui_input3::Modifiers::kNumLock}}),
      ExpectedKeyValue(
          "ArrowLeft", "ArrowLeft", kKeyDown,
          {.modifiers = {fuchsia_ui_input3::Modifiers::kScrollLock}}));
}

IN_PROC_BROWSER_TEST_F(KeyboardInputTest, Disconnect) {
  // Disconnect the keyboard service.
  keyboard_service_.reset();

  frame_for_test_.navigation_listener().RunUntilTitleEquals("loaded");

  // Make sure the page is still available and there are no crashes.
  EXPECT_TRUE(
      ExecuteJavaScript(frame_for_test_.ptr().get(), "true")->GetBool());
}

class KeyboardInputTestWithoutKeyboardFeature : public KeyboardInputTest {
 public:
  KeyboardInputTestWithoutKeyboardFeature() = default;
  ~KeyboardInputTestWithoutKeyboardFeature() override = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({}, {});
    WebEngineBrowserTest::SetUp();
  }

  void SetUpService() override {
    keyboard_input_checker_.emplace(component_context_->additional_services());
  }

  std::optional<NeverConnectedChecker<fuchsia_ui_input3::Keyboard>>
      keyboard_input_checker_;
};

IN_PROC_BROWSER_TEST_F(KeyboardInputTestWithoutKeyboardFeature, NoFeature) {
  // Test will verify that |keyboard_input_checker_| never received a connection
  // request at teardown time.
}

}  // namespace
