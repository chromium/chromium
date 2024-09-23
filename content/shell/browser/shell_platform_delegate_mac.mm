// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_platform_delegate.h"

#import <Cocoa/Cocoa.h>

#include <algorithm>

#import "base/apple/foundation_util.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/app/resource.h"
#include "content/shell/browser/shell.h"
#include "url/gurl.h"

// Receives notification that the window is closing so that it can start the
// tear-down process.
@interface ContentShellWindowDelegate : NSObject <NSWindowDelegate>

@property(readonly) NSWindow* window;

- (id)initWithShell:(content::Shell*)shell window:(NSWindow*)window;
- (void)showDevTools:(id)sender;

@end

@implementation ContentShellWindowDelegate {
  raw_ptr<content::Shell> _shell;
  NSWindow* __strong _window;
}

@synthesize window = _window;

- (id)initWithShell:(content::Shell*)shell window:(NSWindow*)window {
  if ((self = [super init])) {
    _shell = shell;
    _window = window;
    window.delegate = self;
  }
  return self;
}

// Called when the window is about to close. Perform the self-destruction
// sequence by getting rid of the shell and removing it and the window from
// the various global lists. By returning YES, we allow the window to be
// removed from the screen.
- (BOOL)windowShouldClose:(id)sender {
  CHECK_EQ(base::apple::ObjCCastStrict<NSWindow>(sender), _window);
  // Don't leave a dangling pointer if the window lives beyond
  // this method. See crbug.com/719830.
  _window.delegate = nil;
  _window = nil;
  _shell.ClearAndDelete();

  return YES;
}

- (void)performAction:(id)sender {
  _shell->ActionPerformed([sender tag]);
}

- (void)takeURLStringValueFrom:(id)sender {
  _shell->URLEntered(base::SysNSStringToUTF8([sender stringValue]));
}

- (void)showDevTools:(id)sender {
  _shell->ShowDevTools();
}

@end

namespace {

NSString* kWindowTitle = @"Content Shell";

// Layout constants (in view coordinates)
const CGFloat kButtonWidth = 72;
const CGFloat kURLBarHeight = 24;

// The minimum size of the window's content (in view coordinates)
const CGFloat kMinimumWindowWidth = 400;
const CGFloat kMinimumWindowHeight = 300;

void MakeShellButton(NSRect* rect,
                     NSString* title,
                     NSView* parent,
                     int control,
                     id target,
                     NSString* key,
                     NSUInteger modifier) {
  NSButton* button = [[NSButton alloc] initWithFrame:*rect];
  button.title = title;
  button.bezelStyle = NSBezelStyleSmallSquare;
  button.autoresizingMask = (NSViewMaxXMargin | NSViewMinYMargin);
  button.target = target;
  button.action = @selector(performAction:);
  button.tag = control;
  button.keyEquivalent = key;
  button.keyEquivalentModifierMask = modifier;
  [parent addSubview:button];
  rect->origin.x += kButtonWidth;
}

}  // namespace

namespace content {

struct ShellPlatformDelegate::ShellData {
  ContentShellWindowDelegate* __strong delegate;
  NSTextField* __weak url_edit_view;
};

struct ShellPlatformDelegate::PlatformData {};

ShellPlatformDelegate::ShellPlatformDelegate() = default;
ShellPlatformDelegate::~ShellPlatformDelegate() = default;

void ShellPlatformDelegate::Initialize(const gfx::Size& default_window_size) {
  screen_ = std::make_unique<display::ScopedNativeScreen>();
}

void ShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  DCHECK(!base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  int width = initial_size.width();
  int height = initial_size.height();

  if (!Shell::ShouldHideToolbar()) {
    height += kURLBarHeight;
  }
  NSRect initial_window_bounds = NSMakeRect(0, 0, width, height);
  NSRect content_rect = initial_window_bounds;
  NSUInteger style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable;
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:content_rect
                                  styleMask:style_mask
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  window.title = kWindowTitle;
  NSView* content = window.contentView;

  // If the window is allowed to get too small, it will wreck the view bindings.
  NSSize min_size = NSMakeSize(kMinimumWindowWidth, kMinimumWindowHeight);
  min_size = [content convertSize:min_size toView:nil];
  // Note that this takes window coordinates.
  window.contentMinSize = min_size;

  // Set the shell window to participate in fullscreen mode.
  window.collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;

  // Rely on the window delegate to clean us up rather than immediately
  // releasing when the window gets closed. We use the delegate to do everything
  // from the autorelease pool so the shell isn't on the stack during cleanup
  // (ie, a window close from javascript). Also, releasedWhenClosed == YES is
  // incompatible with ARC.
  window.releasedWhenClosed = NO;

  // Create a window delegate to watch for when it's asked to go away.
  ContentShellWindowDelegate* delegate =
      [[ContentShellWindowDelegate alloc] initWithShell:shell window:window];

  if (!Shell::ShouldHideToolbar()) {
    NSRect button_frame =
        NSMakeRect(0, NSMaxY(initial_window_bounds) - kURLBarHeight,
                   kButtonWidth, kURLBarHeight);

    MakeShellButton(&button_frame, @"Back", content, IDC_NAV_BACK, delegate,
                    @"[", NSEventModifierFlagCommand);
    MakeShellButton(&button_frame, @"Forward", content, IDC_NAV_FORWARD,
                    delegate, @"]", NSEventModifierFlagCommand);
    MakeShellButton(&button_frame, @"Reload", content, IDC_NAV_RELOAD, delegate,
                    @"r", NSEventModifierFlagCommand);
    MakeShellButton(&button_frame, @"Stop", content, IDC_NAV_STOP, delegate,
                    @".", NSEventModifierFlagCommand);

    button_frame.size.width =
        NSWidth(initial_window_bounds) - NSMinX(button_frame);
    NSTextField* url_edit_view =
        [[NSTextField alloc] initWithFrame:button_frame];
    [content addSubview:url_edit_view];
    url_edit_view.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    url_edit_view.target = delegate;
    url_edit_view.action = @selector(takeURLStringValueFrom:);
    url_edit_view.cell.wraps = NO;
    url_edit_view.cell.scrollable = YES;
    shell_data.url_edit_view = url_edit_view;
  }

  // Show the new window.
  [window makeKeyAndOrderFront:nil];

  shell_data.delegate = delegate;
}

gfx::NativeWindow ShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  return shell_data.delegate.window;
}

void ShellPlatformDelegate::CleanUp(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  shell_data_map_.erase(shell);
}

void ShellPlatformDelegate::SetContents(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  NSView* web_view = shell->web_contents()->GetNativeView().GetNativeNSView();
  web_view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  NSWindow* window = shell_data.delegate.window;
  [window.contentView addSubview:web_view];

  NSRect frame = window.contentView.bounds;
  if (!Shell::ShouldHideToolbar()) {
    frame.size.height -= kURLBarHeight;
  }
  web_view.frame = frame;
  web_view.needsDisplay = YES;
}

void ShellPlatformDelegate::ResizeWebContent(Shell* shell,
                                             const gfx::Size& content_size) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  int toolbar_height = Shell::ShouldHideToolbar() ? 0 : kURLBarHeight;
  NSRect frame = NSMakeRect(0, 0, content_size.width(),
                            content_size.height() + toolbar_height);
  shell_data.delegate.window.contentView.frame = frame;
}

void ShellPlatformDelegate::EnableUIControl(Shell* shell,
                                            UIControl control,
                                            bool is_enabled) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  int id;
  switch (control) {
    case BACK_BUTTON:
      id = IDC_NAV_BACK;
      break;
    case FORWARD_BUTTON:
      id = IDC_NAV_FORWARD;
      break;
    case STOP_BUTTON:
      id = IDC_NAV_STOP;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown UI control";
      return;
  }
  [[shell_data.delegate.window.contentView viewWithTag:id]
      setEnabled:is_enabled];
}

void ShellPlatformDelegate::SetAddressBarURL(Shell* shell, const GURL& url) {
  if (Shell::ShouldHideToolbar()) {
    return;
  }

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  NSString* url_string = base::SysUTF8ToNSString(url.spec());
  shell_data.url_edit_view.stringValue = url_string;
}

void ShellPlatformDelegate::SetIsLoading(Shell* shell, bool loading) {}

void ShellPlatformDelegate::SetTitle(Shell* shell,
                                     const std::u16string& title) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  shell_data.delegate.window.title = base::SysUTF16ToNSString(title);
}

void ShellPlatformDelegate::MainFrameCreated(Shell* shell) {}

bool ShellPlatformDelegate::DestroyShell(Shell* shell) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  [shell_data.delegate.window performClose:nil];
  return true;  // The performClose() will do the destruction of Shell.
}

void ShellPlatformDelegate::ActivateContents(Shell* shell,
                                             WebContents* top_contents) {
  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  // This focuses the main frame RenderWidgetHost in the window, but does not
  // make the window itself active. The WebContentsDelegate (this class) is
  // responsible for doing both.
  top_contents->Focus();
  // This makes the window the active window for the application, and when the
  // app is active, the window will be also. That makes all RenderWidgetHosts
  // for the window active (which is separate from focused on mac).
  [shell_data.delegate.window makeKeyAndOrderFront:nil];
  // This makes the application active so that we can actually move focus
  // between windows and the renderer can receive focus/blur events.
  [NSApp activateIgnoringOtherApps:YES];
}

void ShellPlatformDelegate::DidNavigatePrimaryMainFramePostCommit(
    Shell* shell,
    WebContents* contents) {}

bool ShellPlatformDelegate::HandleKeyboardEvent(
    Shell* shell,
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.skip_if_unhandled || Shell::ShouldHideToolbar()) {
    return false;
  }

  DCHECK(base::Contains(shell_data_map_, shell));
  ShellData& shell_data = shell_data_map_[shell];

  // The event handling to get this strictly right is a tangle; cheat here a bit
  // by just letting the menus have a chance at it.
  NSEvent* ns_event = event.os_event.Get();
  if (ns_event.type == NSEventTypeKeyDown) {
    if ((ns_event.modifierFlags & NSEventModifierFlagCommand) &&
        [ns_event.characters isEqual:@"l"]) {
      [shell_data.delegate.window makeFirstResponder:shell_data.url_edit_view];
      return true;
    }

    [NSApp.mainMenu performKeyEquivalent:ns_event];
    return true;
  }
  return false;
}

}  // namespace content
