// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller_aura.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_client.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/app_window/test_app_window_contents.h"
#include "extensions/common/extension_builder.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "extensions/shell/test/shell_test_base_aura.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_minimal.h"
#include "ui/display/display.h"
#include "ui/display/screen_base.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/power/fake_power_manager_client.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/events/ozone/events_ozone.h"
#endif

namespace extensions {

class ShellDesktopControllerAuraTest : public ShellTestBaseAura {
 public:
  ShellDesktopControllerAuraTest() = default;

  ShellDesktopControllerAuraTest(const ShellDesktopControllerAuraTest&) =
      delete;
  ShellDesktopControllerAuraTest& operator=(
      const ShellDesktopControllerAuraTest&) = delete;

  ~ShellDesktopControllerAuraTest() override = default;

  void SetUp() override {
    // Set up a screen with 2 displays before `ShellTestBaseAura::SetUp()`
    screen_ = std::make_unique<display::ScreenBase>();
    screen_->display_list().AddDisplay(
        display::Display(100, gfx::Rect(0, 0, 1920, 1080)),
        display::DisplayList::Type::PRIMARY);
    screen_->display_list().AddDisplay(
        display::Display(200, gfx::Rect(1920, 1080, 800, 600)),
        display::DisplayList::Type::NOT_PRIMARY);
    display::Screen::SetScreenInstance(screen_.get());
    ShellTestBaseAura::SetUp();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::PowerManagerClient::InitializeFake();
#endif

    controller_ =
        std::make_unique<ShellDesktopControllerAura>(browser_context());
  }

  void TearDown() override {
    controller_.reset();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::PowerManagerClient::Shutdown();
#endif
    ShellTestBaseAura::TearDown();
    display::Screen::SetScreenInstance(nullptr);
    screen_.reset();
  }

 protected:
  AppWindow* CreateAppWindow(const Extension* extension,
                             gfx::Rect bounds = {}) {
    AppWindow* app_window =
        AppWindowClient::Get()->CreateAppWindow(browser_context(), extension);
    InitAppWindow(app_window, bounds);
    return app_window;
  }

  std::unique_ptr<display::ScreenBase> screen_;
  std::unique_ptr<ShellDesktopControllerAura> controller_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that a shutdown request is sent to the power manager when the power
// button is pressed.
TEST_F(ShellDesktopControllerAuraTest, PowerButton) {
  // Ignore button releases.
  auto* power_manager_client = chromeos::FakePowerManagerClient::Get();
  power_manager_client->SendPowerButtonEvent(false /* down */,
                                             base::TimeTicks());
  EXPECT_EQ(0, power_manager_client->num_request_shutdown_calls());

  // A button press should trigger a shutdown request.
  power_manager_client->SendPowerButtonEvent(true /* down */,
                                             base::TimeTicks());
  EXPECT_EQ(1, power_manager_client->num_request_shutdown_calls());
}
#endif

// Tests that basic input events are handled and forwarded to the host.
// TODO(michaelpg): Test other types of input.
// Flaky on Linux dbg.  http://crbug.com/1516907
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_InputEvents DISABLED_InputEvents
#else
#define MAYBE_InputEvents InputEvents
#endif
TEST_F(ShellDesktopControllerAuraTest, MAYBE_InputEvents) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  CreateAppWindow(extension.get());

  ui::InputMethod* input_method =
      controller_->GetPrimaryHost()->GetInputMethod();
  ASSERT_TRUE(input_method);

  // Set up a focused text input to receive the keypress event.
  ui::DummyTextInputClient client(ui::TEXT_INPUT_TYPE_TEXT);
  input_method->SetFocusedTextInputClient(&client);
  EXPECT_EQ(0, client.insert_char_count());

  // Dispatch a keypress on the window tree host to verify it is processed.
  ui::KeyEvent key_press = ui::KeyEvent::FromCharacter(
      u'a', ui::VKEY_A, ui::DomCode::NONE, ui::EF_NONE);
#if BUILDFLAG(IS_OZONE)
  // Mark IME ignoring flag for ozone platform to be just a key event skipping
  // IME handling, which is referred in some IME handling code based on ozone.
  ui::SetKeyboardImeFlags(&key_press, ui::kPropertyKeyboardImeIgnoredFlag);
#endif
  ui::EventDispatchDetails details =
      controller_->GetPrimaryHost()->dispatcher()->DispatchEvent(
          controller_->GetPrimaryHost()->window(), &key_press);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);
  EXPECT_TRUE(key_press.handled());
  EXPECT_EQ(1, client.insert_char_count());

  // Clean up.
  input_method->DetachTextInputClient(&client);
}

// Tests closing all AppWindows.
TEST_F(ShellDesktopControllerAuraTest, CloseAppWindows) {
  const AppWindowRegistry* app_window_registry =
      AppWindowRegistry::Get(browser_context());
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  for (int i = 0; i < 3; i++)
    CreateAppWindow(extension.get());
  EXPECT_EQ(3u, app_window_registry->app_windows().size());

  controller_->CloseAppWindows();
  EXPECT_EQ(0u, app_window_registry->app_windows().size());
}

// Tests that the AppWindows are removed when the desktop controller goes away.
TEST_F(ShellDesktopControllerAuraTest, OnAppWindowClose) {
  const AppWindowRegistry* app_window_registry =
      AppWindowRegistry::Get(browser_context());
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  for (int i = 0; i < 3; i++)
    CreateAppWindow(extension.get());
  EXPECT_EQ(3u, app_window_registry->app_windows().size());

  // Deleting the controller closes all app windows.
  controller_.reset();
  EXPECT_EQ(0u, app_window_registry->app_windows().size());
}

// Tests that multiple displays result in multiple root windows.
TEST_F(ShellDesktopControllerAuraTest, MultipleDisplays) {
  const AppWindowRegistry* app_window_registry =
      AppWindowRegistry::Get(browser_context());
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();

  // Create two apps window on the primary display. Both should be hosted in the
  // same RootWindowController.
  AppWindow* primary_app_window = CreateAppWindow(extension.get());
  AppWindow* primary_app_window_2 = CreateAppWindow(extension.get());
  EXPECT_EQ(2u, app_window_registry->app_windows().size());
  EXPECT_EQ(1u, controller_->GetAllRootWindows().size());

  // Create an app window near the secondary display, which should result in
  // creating a RootWindowController for that display.
  AppWindow* secondary_app_window =
      CreateAppWindow(extension.get(), gfx::Rect(1900, 1000, 600, 400));
  EXPECT_EQ(3u, app_window_registry->app_windows().size());
  EXPECT_EQ(2u, controller_->GetAllRootWindows().size());

  aura::Window* primary_root = controller_->GetPrimaryHost()->window();

  // Move an app window from the second display to the primary display.
  EXPECT_NE(secondary_app_window->GetNativeWindow()->GetRootWindow(),
            primary_root);
  controller_->SetWindowBoundsInScreen(secondary_app_window,
                                       gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(secondary_app_window->GetNativeWindow()->GetRootWindow(),
            primary_root);

  // Move an app window from the primary display to the secondary display.
  EXPECT_EQ(primary_app_window->GetNativeWindow()->GetRootWindow(),
            primary_root);
  controller_->SetWindowBoundsInScreen(primary_app_window,
                                       gfx::Rect(1920, 1080, 800, 600));
  EXPECT_NE(primary_app_window->GetNativeWindow()->GetRootWindow(),
            primary_root);

  // Test moving an app window within its display.
  EXPECT_EQ(primary_app_window_2->GetNativeWindow()->GetRootWindow(),
            primary_root);
  controller_->SetWindowBoundsInScreen(primary_app_window_2,
                                       gfx::Rect(100, 100, 500, 600));
  EXPECT_EQ(primary_app_window_2->GetNativeWindow()->GetRootWindow(),
            primary_root);
}

}  // namespace extensions
