// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_MAC_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "extensions/shell/browser/shell_native_app_window.h"

namespace extensions {
class ShellNativeAppWindowMac;
}

// A window controller for ShellNativeAppWindowMac to handle NSNotifications
// and pass them to the C++ implementation.
@interface ShellNativeAppWindowController
    : NSWindowController <NSWindowDelegate>

@property(assign, nonatomic) extensions::ShellNativeAppWindowMac* appWindow;

@end

namespace extensions {

// A minimal implementation of ShellNativeAppWindow for Mac Cocoa.
// Based on the NativeAppWindowCocoa implementation.
class ShellNativeAppWindowMac : public ShellNativeAppWindow {
 public:
  ShellNativeAppWindowMac(extensions::AppWindow* app_window,
                          const extensions::AppWindow::CreateParams& params);

  ShellNativeAppWindowMac(const ShellNativeAppWindowMac&) = delete;
  ShellNativeAppWindowMac& operator=(const ShellNativeAppWindowMac&) = delete;

  ~ShellNativeAppWindowMac() override;

  // ui::BaseWindow:
  bool IsActive() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetBounds() const override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  void Activate() override;
  void Deactivate() override;
  void SetBounds(const gfx::Rect& bounds) override;

  // NativeAppWindow:
  gfx::Size GetContentMinimumSize() const override;
  gfx::Size GetContentMaximumSize() const override;

  // Called when the window is about to close.
  void WindowWillClose();

 private:
  NSWindow* window() const;

  ShellNativeAppWindowController* __strong window_controller_;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_NATIVE_APP_WINDOW_MAC_H_
