// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/keep_alive_registry/keep_alive_state_observer.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/root_window_controller.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/display/display.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/display/manager/display_configurator.h"
#endif

namespace aura {
class WindowTreeHost;
}  // namespace aura

namespace base {
class RunLoop;
}  // namespace base

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
#if defined(OS_CHROMEOS)
class UserActivityDetector;
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
#if defined(OS_CHROMEOS)
      public chromeos::PowerManagerClient::Observer,
      public display::DisplayConfigurator::Observer,
#endif
      public ui::internal::InputMethodDelegate,
      public KeepAliveStateObserver {
 public:
  explicit ShellDesktopControllerAura(content::BrowserContext* browser_context);
  ~ShellDesktopControllerAura() override;

  // DesktopController:
  void Run() override;
  void AddAppWindow(AppWindow* app_window, gfx::NativeWindow window) override;
  void CloseAppWindows() override;

  // RootWindowController::DesktopDelegate:
  void CloseRootWindowController(
      RootWindowController* root_window_controller) override;

#if defined(OS_CHROMEOS)
  // chromeos::PowerManagerClient::Observer:
  void PowerButtonEventReceived(bool down,
                                const base::TimeTicks& timestamp) override;

  // display::DisplayConfigurator::Observer:
  void OnDisplayModeChanged(
      const display::DisplayConfigurator::DisplayStateList& displays) override;
#endif

  // ui::internal::InputMethodDelegate:
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

#if defined(OS_CHROMEOS)
  // Returns the desired dimensions of the RootWindowController from the command
  // line, or falls back to a default size.
  gfx::Size GetStartingWindowSize();

  // Returns the dimensions (in pixels) of the primary display, or an empty size
  // if the dimensions can't be determined or no display is connected.
  gfx::Size GetPrimaryDisplaySize();
#endif

  content::BrowserContext* const browser_context_;

#if defined(OS_CHROMEOS)
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

#if defined(OS_CHROMEOS)
  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;
  std::unique_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
#endif

  std::unique_ptr<AppWindowClient> app_window_client_;

  // NativeAppWindow::Close() deletes the AppWindow.
  std::list<AppWindow*> app_windows_;

  // A pointer to the main message loop if this is run by ShellBrowserMainParts.
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShellDesktopControllerAura);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_DESKTOP_CONTROLLER_AURA_H_
