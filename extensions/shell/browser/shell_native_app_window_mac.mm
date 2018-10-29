// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "extensions/shell/browser/shell_native_app_window_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#import "ui/gfx/mac/coordinate_conversion.h"

@implementation ShellNativeAppWindowController

@synthesize appWindow = appWindow_;

- (void)windowWillClose:(NSNotification*)notification {
  if (appWindow_)
    appWindow_->WindowWillClose();
}

@end

// TODO(yoz): Do we need to handle commands (keyboard shortcuts)?
// Do we need need ChromeEventProcessingWindow or UnderlayOpenGLHostingWindow?
@interface ShellNSWindow : NSWindow
@end

@implementation ShellNSWindow

- (BOOL)_isTitleHidden {
  return YES;
}

@end

namespace extensions {

ShellNativeAppWindowMac::ShellNativeAppWindowMac(
    AppWindow* app_window,
    const AppWindow::CreateParams& params)
    : ShellNativeAppWindow(app_window, params) {
  base::scoped_nsobject<NSWindow> shell_window;
  NSUInteger style_mask = NSTitledWindowMask | NSClosableWindowMask;

  NSRect cocoa_bounds = gfx::ScreenRectToNSRect(
      params.GetInitialWindowBounds(gfx::Insets()));

  shell_window.reset(
      [[ShellNSWindow alloc] initWithContentRect:cocoa_bounds
                                       styleMask:style_mask
                                         backing:NSBackingStoreBuffered
                                           defer:NO]);
  [shell_window setReleasedWhenClosed:NO];

  window_controller_.reset([[ShellNativeAppWindowController alloc]
                            initWithWindow:shell_window]);

  [[window_controller_ window] setDelegate:window_controller_];
  [window_controller_ setAppWindow:this];

  NSView* view = app_window->web_contents()->GetNativeView().GetNativeNSView();
  NSView* frameView = [window() contentView];
  [view setFrame:[frameView bounds]];
  [frameView addSubview:view];
}

ShellNativeAppWindowMac::~ShellNativeAppWindowMac() {
  [window() setDelegate:nil];
  [window() close];
}

bool ShellNativeAppWindowMac::IsActive() const {
  return [window() isKeyWindow];
}

gfx::NativeWindow ShellNativeAppWindowMac::GetNativeWindow() const {
  return window();
}

gfx::Rect ShellNativeAppWindowMac::GetBounds() const {
  return gfx::ScreenRectFromNSRect([window() frame]);
}

void ShellNativeAppWindowMac::Show() {
  [window_controller_ showWindow:nil];
}

void ShellNativeAppWindowMac::Hide() {
  NOTIMPLEMENTED();
}

bool ShellNativeAppWindowMac::IsVisible() const {
  return [window() isVisible];
}

void ShellNativeAppWindowMac::Activate() {
  // TODO(yoz): Activate in front of other applications.
  [[window_controller_ window] makeKeyAndOrderFront:window_controller_];
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
  [window_controller_ setAppWindow:NULL];
  app_window()->OnNativeWindowChanged();
  app_window()->OnNativeClose();
}

ShellNSWindow* ShellNativeAppWindowMac::window() const {
  return base::mac::ObjCCastStrict<ShellNSWindow>([window_controller_ window]);
}

}  // namespace extensions
