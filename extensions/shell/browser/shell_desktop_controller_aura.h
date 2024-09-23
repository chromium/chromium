// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/keep_alive_registry/keep_alive_state_observer.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/root_window_controller.h"
#include "ui/aura/window.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/display/display.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/display/manager/display_configurator.h"
#endif

namespace aura {
class WindowTreeHost;
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace display {
class Screen;
}  // namespace display

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {
class InputMethod;
#if BUILDFLAG(IS_CHROMEOS_ASH)
class UserActivityPowerManagerNotifier;
#endif
}  // namespace ui

namespace wm {
class CompoundEventFilter;
class CursorManager;
class FocusController;
}  // namespace wm

namespace extensions {
class AppWindowClient;

// Simple desktop controller for app_shell. Associates each display with a
// RootWindowController. Adds AppWindows by passing them to the nearest
// RootWindowController.
class ShellDesktopControllerAura
    : public DesktopController,
      public RootWindowController::DesktopDelegate,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      public chromeos::PowerManagerClient::Observer,
      public display::DisplayConfigurator::Observer,
#endif
      public ui::ImeKeyEventDispatcher,
      public KeepAliveStateObserver {
 public:
  explicit ShellDesktopControllerAura(content::BrowserContext* browser_context);

  ShellDesktopControllerAura(const ShellDesktopControllerAura&) = delete;
  ShellDesktopControllerAura& operator=(const ShellDesktopControllerAura&) =
      delete;

  ~ShellDesktopControllerAura() override;

  // DesktopController:
  void PreMainMessageLoopRun() override;
  void WillRunMainMessageLoop(
      std::unique_ptr<base::RunLoop>& run_loop) override;
  void PostMainMessageLoopRun() override;
  void AddAppWindow(AppWindow* app_window, gfx::NativeWindow window) override;
  void CloseAppWindows() override;

  // RootWindowController::DesktopDelegate:
  void CloseRootWindowController(
      RootWindowController* root_window_controller) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // chromeos::PowerManagerClient::Observer:
  void PowerButtonEventReceived(bool down, base::TimeTicks timestamp) override;

  // display::DisplayConfigurator::Observer:
  void OnDisplayConfigurationChanged(
      const display::DisplayConfigurator::DisplayStateList& displays) override;
#endif

  // ui::ImeKeyEventDispatcher:
  ui::EventDispatchDetails DispatchKeyEventPostIME(
      ui::KeyEvent* key_event) override;

  // KeepAliveStateObserver:
  void OnKeepAliveStateChanged(bool is_keeping_alive) override;
  void OnKeepAliveRestartStateChanged(bool can_restart) override;

  // Returns the WindowTreeHost for the primary display.
  aura::WindowTreeHost* GetPrimaryHost();

  // Returns all root windows managed by RootWindowControllers.
  aura::Window::Windows GetAllRootWindows();

  // Updates the bounds of |app_window|. This may involve reparenting the window
  // to a different root window if the new bounds are in a different display.
  void SetWindowBoundsInScreen(AppWindow* app_window, const gfx::Rect& bounds);

 protected:
  // Creates and sets the aura clients and window manager stuff. Subclass may
  // initialize different sets of the clients.
  virtual void InitWindowManager();

  // Removes all RootWindowControllers and tears down our aura clients.
  virtual void TearDownWindowManager();

 private:
  // Creates a RootWindowController to host AppWindows.
  std::unique_ptr<RootWindowController> CreateRootWindowControllerForDisplay(
      const display::Display& display);

  // Removes handlers from the RootWindowController so it can be destroyed.
  void TearDownRootWindowController(RootWindowController* root);

  // Quits if there are no app windows, and no keep-alives waiting for apps to
  // relaunch.
  void MaybeQuit();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns the desired dimensions of the RootWindowController from the command
  // line, or falls back to a default size.
  gfx::Size GetStartingWindowSize();

  // Returns the dimensions (in pixels) of the primary display, or an empty size
  // if the dimensions can't be determined or no display is connected.
  gfx::Size GetPrimaryDisplaySize();
#endif

  const raw_ptr<content::BrowserContext> browser_context_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<display::DisplayConfigurator> display_configurator_;
#endif

  std::unique_ptr<display::Screen> screen_;

  std::unique_ptr<wm::CompoundEventFilter> root_window_event_filter_;

  // Mapping from display ID to the RootWindowController created for that
  // display.
  std::map<int64_t, std::unique_ptr<RootWindowController>>
      root_window_controllers_;

  std::unique_ptr<ui::InputMethod> input_method_;

  std::unique_ptr<wm::FocusController> focus_controller_;

  std::unique_ptr<wm::CursorManager> cursor_manager_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
#endif

  std::unique_ptr<AppWindowClient> app_window_client_;

  // NativeAppWindow::Close() deletes the AppWindow.
  std::list<raw_ptr<AppWindow, CtnExperimental>> app_windows_;

  // Non-null between WillRunMainMessageLoop() and MaybeQuit().
  base::OnceClosure quit_when_idle_closure_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_
