// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller_aura.h"

#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "build/chromeos_buildflags.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/shell/browser/shell_app_window_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/base/ime/input_method.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/cursor_loader.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/command_line.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "extensions/shell/browser/shell_screen.h"
#include "extensions/shell/common/switches.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/public/ozone_platform.h"  // nogncheck
#else
#include "ui/views/widget/desktop_aura/desktop_screen.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {

// A class that bridges the gap between CursorManager and Aura. It borrows
// heavily from NativeCursorManagerAsh.
class ShellNativeCursorManager : public wm::NativeCursorManager {
 public:
  explicit ShellNativeCursorManager(
      ShellDesktopControllerAura* desktop_controller)
      : desktop_controller_(desktop_controller) {
    aura::client::SetCursorShapeClient(&cursor_loader_);
  }

  ShellNativeCursorManager(const ShellNativeCursorManager&) = delete;
  ShellNativeCursorManager& operator=(const ShellNativeCursorManager&) = delete;

  ~ShellNativeCursorManager() override {
    aura::client::SetCursorShapeClient(nullptr);
  }

  // wm::NativeCursorManager overrides.
  void SetDisplay(const display::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override {
    if (cursor_loader_.SetDisplay(display)) {
      SetCursor(delegate->GetCursor(), delegate);
    }
  }

  void SetCursor(gfx::NativeCursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override {
    cursor_loader_.SetPlatformCursor(&cursor);
    delegate->CommitCursor(cursor);

    if (delegate->IsCursorVisible())
      SetCursorOnAllRootWindows(cursor);
  }

  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitVisibility(visible);

    if (visible) {
      SetCursor(delegate->GetCursor(), delegate);
    } else {
      gfx::NativeCursor invisible_cursor(ui::mojom::CursorType::kNone);
      cursor_loader_.SetPlatformCursor(&invisible_cursor);
      SetCursorOnAllRootWindows(invisible_cursor);
    }
  }

  void SetCursorSize(ui::CursorSize cursor_size,
                     wm::NativeCursorManagerDelegate* delegate) override {
    cursor_loader_.SetSize(cursor_size);
    delegate->CommitCursorSize(cursor_size);
    if (delegate->IsCursorVisible())
      SetCursor(delegate->GetCursor(), delegate);
  }

  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitMouseEventsEnabled(enabled);
    SetVisibility(delegate->IsCursorVisible(), delegate);
  }

 private:
  // Sets |cursor| as the active cursor within Aura.
  void SetCursorOnAllRootWindows(gfx::NativeCursor cursor) {
    for (aura::Window* window : desktop_controller_->GetAllRootWindows()) {
      window->GetHost()->SetCursor(cursor);
    }
  }

  raw_ptr<ShellDesktopControllerAura> desktop_controller_;  // Not owned.

  wm::CursorLoader cursor_loader_{/*use_platform_cursors=*/false};
};

class AppsFocusRules : public wm::BaseFocusRules {
 public:
  AppsFocusRules() {}

  AppsFocusRules(const AppsFocusRules&) = delete;
  AppsFocusRules& operator=(const AppsFocusRules&) = delete;

  ~AppsFocusRules() override {}

  bool SupportsChildActivation(const aura::Window* window) const override {
    return true;
  }
};

}  // namespace

ShellDesktopControllerAura::ShellDesktopControllerAura(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      app_window_client_(new ShellAppWindowClient) {
  extensions::AppWindowClient::Set(app_window_client_.get());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  display_configurator_ = std::make_unique<display::DisplayConfigurator>();
  display_configurator_->Init(
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(), false);
  display_configurator_->ForceInitialConfigure();
  display_configurator_->AddObserver(this);
#endif

  InitWindowManager();
}

ShellDesktopControllerAura::~ShellDesktopControllerAura() {
  TearDownWindowManager();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
#endif
  extensions::AppWindowClient::Set(nullptr);
}

void ShellDesktopControllerAura::PreMainMessageLoopRun() {
  KeepAliveRegistry::GetInstance()->AddObserver(this);
}

void ShellDesktopControllerAura::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  quit_when_idle_closure_ = run_loop->QuitWhenIdleClosure();
}

void ShellDesktopControllerAura::PostMainMessageLoopRun() {
  KeepAliveRegistry::GetInstance()->SetIsShuttingDown(true);
  KeepAliveRegistry::GetInstance()->RemoveObserver(this);
}

void ShellDesktopControllerAura::AddAppWindow(AppWindow* app_window,
                                              gfx::NativeWindow window) {
  // Find the closest display to the specified bounds.
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayMatching(
          window->GetBoundsInScreen());

  // Create a RootWindowController for the display if necessary.
  if (root_window_controllers_.count(display.id()) == 0) {
    root_window_controllers_[display.id()] =
        CreateRootWindowControllerForDisplay(display);
  }
  root_window_controllers_[display.id()]->AddAppWindow(app_window, window);
}

void ShellDesktopControllerAura::CloseAppWindows() {
  for (auto& pair : root_window_controllers_)
    pair.second->CloseAppWindows();
}

void ShellDesktopControllerAura::CloseRootWindowController(
    RootWindowController* root_window_controller) {
  const auto it = base::ranges::find(
      root_window_controllers_, root_window_controller,
      [](const auto& candidate_pair) { return candidate_pair.second.get(); });
  CHECK(it != root_window_controllers_.end(), base::NotFatalUntil::M130);
  TearDownRootWindowController(it->second.get());
  root_window_controllers_.erase(it);

  MaybeQuit();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ShellDesktopControllerAura::PowerButtonEventReceived(
    bool down,
    base::TimeTicks timestamp) {
  if (down) {
    chromeos::PowerManagerClient::Get()->RequestShutdown(
        power_manager::REQUEST_SHUTDOWN_FOR_USER, "AppShell power button");
  }
}

void ShellDesktopControllerAura::OnDisplayConfigurationChanged(
    const display::DisplayConfigurator::DisplayStateList& displays) {
  for (const display::DisplaySnapshot* display_mode : displays) {
    if (!display_mode->current_mode())
      continue;
    auto it = root_window_controllers_.find(display_mode->display_id());
    if (it != root_window_controllers_.end())
      it->second->UpdateSize(display_mode->current_mode()->size());
  }
}
#endif

ui::EventDispatchDetails ShellDesktopControllerAura::DispatchKeyEventPostIME(
    ui::KeyEvent* key_event) {
  if (key_event->target()) {
    aura::WindowTreeHost* host = static_cast<aura::Window*>(key_event->target())
                                     ->GetRootWindow()
                                     ->GetHost();
    return host->DispatchKeyEventPostIME(key_event);
  }

  // Send the key event to the focused window.
  aura::Window* active_window =
      const_cast<aura::Window*>(focus_controller_->GetActiveWindow());
  if (active_window) {
    return active_window->GetRootWindow()->GetHost()->DispatchKeyEventPostIME(
        key_event);
  }

  return GetPrimaryHost()->DispatchKeyEventPostIME(key_event);
}

void ShellDesktopControllerAura::OnKeepAliveStateChanged(
    bool is_keeping_alive) {
  if (!is_keeping_alive)
    MaybeQuit();
}

void ShellDesktopControllerAura::OnKeepAliveRestartStateChanged(
    bool can_restart) {}

aura::WindowTreeHost* ShellDesktopControllerAura::GetPrimaryHost() {
  if (root_window_controllers_.empty())
    return nullptr;

  const display::Display& display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  if (root_window_controllers_.count(display.id()) == 1)
    return root_window_controllers_[display.id()]->host();

  // Fall back to an existing host.
  return root_window_controllers_.begin()->second->host();
}

aura::Window::Windows ShellDesktopControllerAura::GetAllRootWindows() {
  aura::Window::Windows windows;
  for (auto& pair : root_window_controllers_)
    windows.push_back(pair.second->host()->window());
  return windows;
}

void ShellDesktopControllerAura::SetWindowBoundsInScreen(
    AppWindow* app_window,
    const gfx::Rect& bounds) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);

  // Create a RootWindowController for the display if necessary.
  if (root_window_controllers_.count(display.id()) == 0) {
    root_window_controllers_[display.id()] =
        CreateRootWindowControllerForDisplay(display);
  }

  // Check if the window is parented to a different RootWindowController.
  if (app_window->GetNativeWindow()->GetRootWindow() !=
      root_window_controllers_[display.id()]->host()->window()) {
    // Move the window to the appropriate RootWindowController for the display.
    for (const auto& it : root_window_controllers_) {
      if (it.second->host()->window() ==
          app_window->GetNativeWindow()->GetRootWindow()) {
        it.second->RemoveAppWindow(app_window);
        break;
      }
    }
    root_window_controllers_[display.id()]->AddAppWindow(
        app_window, app_window->GetNativeWindow());
  }

  app_window->GetNativeWindow()->SetBoundsInScreen(bounds, display);
}

void ShellDesktopControllerAura::InitWindowManager() {
  root_window_event_filter_ = std::make_unique<wm::CompoundEventFilter>();

  // Screen may be initialized in tests.
  if (!display::Screen::GetScreen()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    screen_ = std::make_unique<ShellScreen>(this, GetStartingWindowSize());
    // TODO(pkasting): Make ShellScreen() call SetScreenInstance() as the
    // classes in CreateDesktopScreen() do, and remove this.
    display::Screen::SetScreenInstance(screen_.get());
#else
    // TODO(crbug.com/40535820): Refactor DesktopScreen out of views.
    screen_ = views::CreateDesktopScreen();
#endif
  }

  focus_controller_ =
      std::make_unique<wm::FocusController>(new AppsFocusRules());
  cursor_manager_ = std::make_unique<wm::CursorManager>(
      std::make_unique<ShellNativeCursorManager>(this));
  cursor_manager_->SetDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay());
  cursor_manager_->SetCursor(ui::mojom::CursorType::kPointer);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_activity_notifier_ =
      std::make_unique<ui::UserActivityPowerManagerNotifier>(
          ui::UserActivityDetector::Get(), /*fingerprint=*/mojo::NullRemote());
#endif
}

void ShellDesktopControllerAura::TearDownWindowManager() {
  for (auto& pair : root_window_controllers_)
    TearDownRootWindowController(pair.second.get());
  root_window_controllers_.clear();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  user_activity_notifier_.reset();
#endif
  cursor_manager_.reset();
  focus_controller_.reset();
  if (screen_) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    display::Screen::SetScreenInstance(nullptr);
#endif
    screen_.reset();
  }
  root_window_event_filter_.reset();
}

std::unique_ptr<RootWindowController>
ShellDesktopControllerAura::CreateRootWindowControllerForDisplay(
    const display::Display& display) {
  // Convert display's bounds from DIP to physical pixels for WindowTreeHost.
  gfx::Rect bounds(gfx::ScaleToFlooredPoint(display.bounds().origin(),
                                            display.device_scale_factor()),
                   display.GetSizeInPixel());
  auto root_window_controller =
      std::make_unique<RootWindowController>(this, bounds, browser_context_);

  // Initialize the root window with our clients.
  aura::Window* root_window = root_window_controller->host()->window();
  root_window->AddPreTargetHandler(root_window_event_filter_.get());
  aura::client::SetFocusClient(root_window, focus_controller_.get());
  root_window->AddPreTargetHandler(focus_controller_.get());
  wm::SetActivationClient(root_window, focus_controller_.get());
  aura::client::SetCursorClient(root_window, cursor_manager_.get());

  if (!input_method_) {
    // Create an input method and become its key event dispatcher.
    input_method_ = ui::CreateInputMethod(
        this, root_window_controller->host()->GetAcceleratedWidget());
    root_window_controller->host()->SetSharedInputMethod(input_method_.get());
  }

  return root_window_controller;
}

void ShellDesktopControllerAura::TearDownRootWindowController(
    RootWindowController* root) {
  root->host()->window()->RemovePreTargetHandler(
      root_window_event_filter_.get());
  root->host()->window()->RemovePreTargetHandler(focus_controller_.get());
}

void ShellDesktopControllerAura::MaybeQuit() {
  // Quit if there are no app windows open and no keep-alives waiting for apps
  // to relaunch.  |run_loop_| may be null in tests.
  if (quit_when_idle_closure_ && root_window_controllers_.empty() &&
      !KeepAliveRegistry::GetInstance()->IsKeepingAlive()) {
    std::move(quit_when_idle_closure_).Run();
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
gfx::Size ShellDesktopControllerAura::GetStartingWindowSize() {
  gfx::Size size = GetPrimaryDisplaySize();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAppShellHostWindowSize)) {
    const std::string size_str =
        command_line->GetSwitchValueASCII(switches::kAppShellHostWindowSize);
    int width, height;
    CHECK_EQ(2, sscanf(size_str.c_str(), "%dx%d", &width, &height));
    size = gfx::Size(width, height);
  }
  return size.IsEmpty() ? gfx::Size(1920, 1080) : size;
}

gfx::Size ShellDesktopControllerAura::GetPrimaryDisplaySize() {
  const display::DisplayConfigurator::DisplayStateList& displays =
      display_configurator_->cached_displays();
  const display::DisplayMode* mode =
      displays.empty() ? nullptr : displays[0]->current_mode();
  return mode ? mode->size() : gfx::Size();
}
#endif

}  // namespace extensions
