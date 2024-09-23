// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_ROOT_WINDOW_CONTROLLER_H_
#define EXTENSIONS_SHELL_BROWSER_ROOT_WINDOW_CONTROLLER_H_

#include <list>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/gfx/native_widget_types.h"

namespace aura {
class WindowTreeHost;
namespace client {
class ScreenPositionClient;
}  // namespace client
}  // namespace aura

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace extensions {
class AppWindow;

// Owns and manages a WindowTreeHost for a display. New AppWindows will fill
// the entire root window. Any additional AppWindows are simply drawn over the
// existing AppWindow(s) and cannot be resized except by resizing the
// WindowTreeHost.
// TODO(michaelpg): Allow app windows to move between displays when bounds are
// updated via the chrome.app.window API.
class RootWindowController : public aura::client::WindowParentingClient,
                             public aura::WindowTreeHostObserver,
                             public AppWindowRegistry::Observer {
 public:
  class DesktopDelegate {
   public:
    virtual ~DesktopDelegate() = default;

    // Called when the root window requests to be closed. This should eventually
    // destroy |root_window_controller|.
    virtual void CloseRootWindowController(
        RootWindowController* root_window_controller) = 0;
  };

  // RootWindowController initializes and displays a WindowTreeHost within
  // |bounds| (in physical pixels).
  // |desktop_delegate| must outlive the RootWindowController.
  RootWindowController(DesktopDelegate* desktop_delegate,
                       const gfx::Rect& bounds,
                       content::BrowserContext* browser_context);

  RootWindowController(const RootWindowController&) = delete;
  RootWindowController& operator=(const RootWindowController&) = delete;

  ~RootWindowController() override;

  // Attaches a NativeAppWindow's window to our root window.
  void AddAppWindow(AppWindow* app_window, gfx::NativeWindow window);

  // Unparents the AppWindow's window from our root window so it can be added to
  // a different RootWindowController.
  void RemoveAppWindow(AppWindow* app_window);

  // Closes the root window's AppWindows, resulting in their destruction.
  void CloseAppWindows();

  // Updates the size of the root window.
  // TODO(michaelpg): Handle display events to adapt or close the window.
  void UpdateSize(const gfx::Size& size);

  aura::WindowTreeHost* host() { return host_.get(); }

  // aura::client::WindowParentingClient:
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds,
                                 const int64_t display_id) override;

  // aura::WindowTreeHostObserver:
  void OnHostCloseRequested(aura::WindowTreeHost* host) override;

  // AppWindowRegistry::Observer:
  void OnAppWindowRemoved(AppWindow* app_window) override;

 private:
  void DestroyWindowTreeHost();

  const raw_ptr<DesktopDelegate> desktop_delegate_;

  // The BrowserContext used to create AppWindows.
  const raw_ptr<content::BrowserContext> browser_context_;

  std::unique_ptr<aura::client::ScreenPositionClient> screen_position_client_;

  // The host we create.
  std::unique_ptr<aura::WindowTreeHost> host_;

  // List of AppWindows we've created. Used to close any remaining app windows
  // when |host_| is closed or |this| is destroyed.
  // Note: Pointers are unowned. NativeAppWindow::Close() will delete them.
  std::list<raw_ptr<AppWindow, CtnExperimental>> app_windows_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_ROOT_WINDOW_CONTROLLER_H_
