// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "extensions/shell/browser/shell_native_app_window_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#import "ui/gfx/mac/coordinate_conversion.h"

@implementation ShellNativeAppWindowController {
  // This field is not a raw_ptr<> because it is a pointer to Objective-C
  // object.
  RAW_PTR_EXCLUSION extensions::ShellNativeAppWindowMac*
      _appWindow;  // Owns us.
}

@synthesize appWindow = _appWindow;

- (void)windowWillClose:(NSNotification*)notification {
  if (_appWindow)
    _appWindow->WindowWillClose();
}

@end

namespace extensions {

ShellNativeAppWindowMac::ShellNativeAppWindowMac(
    AppWindow* app_window,
    const AppWindow::CreateParams& params)
    : ShellNativeAppWindow(app_window, params) {
  NSRect cocoa_bounds = gfx::ScreenRectToNSRect(
      params.GetInitialWindowBounds(gfx::Insets(), gfx::RoundedCornersF()));

  // TODO(yoz): Do we need to handle commands (keyboard shortcuts)?
  // Do we need need ChromeEventProcessingWindow?
  NSWindow* shell_window = [[NSWindow alloc]
      initWithContentRect:cocoa_bounds
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  shell_window.releasedWhenClosed = NO;
  shell_window.titleVisibility = NSWindowTitleHidden;

  window_controller_ =
      [[ShellNativeAppWindowController alloc] initWithWindow:shell_window];

  window().delegate = window_controller_;
  window_controller_.appWindow = this;

  NSView* view = app_window->web_contents()->GetNativeView().GetNativeNSView();
  NSView* frameView = window().contentView;
  view.frame = frameView.bounds;
  [frameView addSubview:view];
}

ShellNativeAppWindowMac::~ShellNativeAppWindowMac() {
  window().delegate = nil;
  [window() close];
}

bool ShellNativeAppWindowMac::IsActive() const {
  return window().keyWindow;
}

gfx::NativeWindow ShellNativeAppWindowMac::GetNativeWindow() const {
  return window();
}

gfx::Rect ShellNativeAppWindowMac::GetBounds() const {
  return gfx::ScreenRectFromNSRect(window().frame);
}

void ShellNativeAppWindowMac::Show() {
  [window_controller_ showWindow:nil];
}

void ShellNativeAppWindowMac::Hide() {
  NOTIMPLEMENTED();
}

bool ShellNativeAppWindowMac::IsVisible() const {
  return window().visible;
}

void ShellNativeAppWindowMac::Activate() {
  // TODO(yoz): Activate in front of other applications.
  [window() makeKeyAndOrderFront:window_controller_];
}

void ShellNativeAppWindowMac::Deactivate() {
  // See crbug.com/51364.
  NOTIMPLEMENTED();
}

void ShellNativeAppWindowMac::SetBounds(const gfx::Rect& bounds) {
  // TODO(yoz): Windows should be fullscreen.
  NOTIMPLEMENTED();
}

gfx::Size ShellNativeAppWindowMac::GetContentMinimumSize() const {
  // Content fills the display and cannot be resized.
  return display::Screen::GetScreen()->GetPrimaryDisplay().bounds().size();
}

gfx::Size ShellNativeAppWindowMac::GetContentMaximumSize() const {
  return GetContentMinimumSize();
}

void ShellNativeAppWindowMac::WindowWillClose() {
  window_controller_.appWindow = nullptr;
  app_window()->OnNativeWindowChanged();
  app_window()->OnNativeClose();
}

NSWindow* ShellNativeAppWindowMac::window() const {
  return window_controller_.window;
}

}  // namespace extensions
