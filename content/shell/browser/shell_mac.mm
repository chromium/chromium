// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell.h"

#include <algorithm>

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/string_piece.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/app/resource.h"
#import "ui/base/cocoa/underlay_opengl_hosting_window.h"
#include "url/gurl.h"

// Receives notification that the window is closing so that it can start the
// tear-down process. Is responsible for deleting itself when done.
@interface ContentShellWindowDelegate : NSObject<NSWindowDelegate> {
 @private
  content::Shell* shell_;
}
- (id)initWithShell:(content::Shell*)shell;
@end

@implementation ContentShellWindowDelegate

- (id)initWithShell:(content::Shell*)shell {
  if ((self = [super init])) {
    shell_ = shell;
  }
  return self;
}

// Called when the window is about to close. Perform the self-destruction
// sequence by getting rid of the shell and removing it and the window from
// the various global lists. By returning YES, we allow the window to be
// removed from the screen.
- (BOOL)windowShouldClose:(id)sender {
  NSWindow* window = base::mac::ObjCCastStrict<NSWindow>(sender);
  [window autorelease];
  // Don't leave a dangling pointer if the window lives beyond
  // this method. See crbug.com/719830.
  [window setDelegate:nil];
  delete shell_;
  [self release];

  return YES;
}

- (void)performAction:(id)sender {
  shell_->ActionPerformed([sender tag]);
}

- (void)takeURLStringValueFrom:(id)sender {
  shell_->URLEntered(base::SysNSStringToUTF8([sender stringValue]));
}

@end

@interface CrShellWindow : UnderlayOpenGLHostingWindow {
 @private
  content::Shell* shell_;
}
- (void)setShell:(content::Shell*)shell;
- (void)showDevTools:(id)sender;
@end

@implementation CrShellWindow

- (void)setShell:(content::Shell*)shell {
  shell_ = shell;
}

- (void)showDevTools:(id)sender {
  shell_->ShowDevTools();
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
                     NSView* target,
                     NSString* key,
                     NSUInteger modifier) {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:*rect]);
  [button setTitle:title];
  [button setBezelStyle:NSSmallSquareBezelStyle];
  [button setAutoresizingMask:(NSViewMaxXMargin | NSViewMinYMargin)];
  [button setTarget:target];
  [button setAction:@selector(performAction:)];
  [button setTag:control];
  [button setKeyEquivalent:key];
  [button setKeyEquivalentModifierMask:modifier];
  [parent addSubview:button];
  rect->origin.x += kButtonWidth;
}

}  // namespace

namespace content {

void Shell::PlatformInitialize(const gfx::Size& default_window_size) {
}

void Shell::PlatformExit() {
}

void Shell::PlatformCleanUp() {
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  if (headless_)
    return;

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
      NOTREACHED() << "Unknown UI control";
      return;
  }
  [[[window_.GetNativeNSWindow() contentView] viewWithTag:id]
      setEnabled:is_enabled];
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
  if (headless_ || hide_toolbar_)
    return;

  NSString* url_string = base::SysUTF8ToNSString(url.spec());
  [url_edit_view_ setStringValue:url_string];
}

void Shell::PlatformSetIsLoading(bool loading) {
}

void Shell::PlatformCreateWindow(int width, int height) {
  if (headless_) {
    content_size_ = gfx::Size(width, height);
    return;
  }

  if (!hide_toolbar_)
    height += kURLBarHeight;
  NSRect initial_window_bounds = NSMakeRect(0, 0, width, height);
  NSRect content_rect = initial_window_bounds;
  NSUInteger style_mask = NSTitledWindowMask |
                          NSClosableWindowMask |
                          NSMiniaturizableWindowMask |
                          NSResizableWindowMask;
  CrShellWindow* window =
      [[CrShellWindow alloc] initWithContentRect:content_rect
                                       styleMask:style_mask
                                         backing:NSBackingStoreBuffered
                                           defer:NO];
  window_ = window;
  [window setShell:this];
  [window_.GetNativeNSWindow() setTitle:kWindowTitle];
  NSView* content = [window_.GetNativeNSWindow() contentView];

  // If the window is allowed to get too small, it will wreck the view bindings.
  NSSize min_size = NSMakeSize(kMinimumWindowWidth, kMinimumWindowHeight);
  min_size = [content convertSize:min_size toView:nil];
  // Note that this takes window coordinates.
  [window_.GetNativeNSWindow() setContentMinSize:min_size];

  // Set the shell window to participate in Lion Fullscreen mode. Set
  // Setting this flag has no effect on Snow Leopard or earlier.
  NSUInteger collectionBehavior =
      [window_.GetNativeNSWindow() collectionBehavior];
  collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
  [window_.GetNativeNSWindow() setCollectionBehavior:collectionBehavior];

  // Rely on the window delegate to clean us up rather than immediately
  // releasing when the window gets closed. We use the delegate to do
  // everything from the autorelease pool so the shell isn't on the stack
  // during cleanup (ie, a window close from javascript).
  [window_.GetNativeNSWindow() setReleasedWhenClosed:NO];

  // Create a window delegate to watch for when it's asked to go away. It will
  // clean itself up so we don't need to hold a reference.
  ContentShellWindowDelegate* delegate =
      [[ContentShellWindowDelegate alloc] initWithShell:this];
  [window_.GetNativeNSWindow() setDelegate:delegate];

  if (!hide_toolbar_) {
    NSRect button_frame =
        NSMakeRect(0, NSMaxY(initial_window_bounds) - kURLBarHeight,
                   kButtonWidth, kURLBarHeight);

    MakeShellButton(&button_frame, @"Back", content, IDC_NAV_BACK,
                    (NSView*)delegate, @"[", NSCommandKeyMask);
    MakeShellButton(&button_frame, @"Forward", content, IDC_NAV_FORWARD,
                    (NSView*)delegate, @"]", NSCommandKeyMask);
    MakeShellButton(&button_frame, @"Reload", content, IDC_NAV_RELOAD,
                    (NSView*)delegate, @"r", NSCommandKeyMask);
    MakeShellButton(&button_frame, @"Stop", content, IDC_NAV_STOP,
                    (NSView*)delegate, @".", NSCommandKeyMask);

    button_frame.size.width =
        NSWidth(initial_window_bounds) - NSMinX(button_frame);
    base::scoped_nsobject<NSTextField> url_edit_view(
        [[NSTextField alloc] initWithFrame:button_frame]);
    [content addSubview:url_edit_view];
    [url_edit_view setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];
    [url_edit_view setTarget:delegate];
    [url_edit_view setAction:@selector(takeURLStringValueFrom:)];
    [[url_edit_view cell] setWraps:NO];
    [[url_edit_view cell] setScrollable:YES];
    url_edit_view_ = url_edit_view.get();
  }

  // show the window
  [window_.GetNativeNSWindow() makeKeyAndOrderFront:nil];
}

void Shell::PlatformSetContents() {
  NSView* web_view = web_contents_->GetNativeView().GetNativeNSView();
  [web_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

  if (headless_) {
    SizeTo(content_size_);
    return;
  }

  NSView* content = [window_.GetNativeNSWindow() contentView];
  [content addSubview:web_view];

  NSRect frame = [content bounds];
  if (!hide_toolbar_)
    frame.size.height -= kURLBarHeight;
  [web_view setFrame:frame];
  [web_view setNeedsDisplay:YES];
}

void Shell::SizeTo(const gfx::Size& content_size) {
  if (!headless_) {
    int toolbar_height = hide_toolbar_ ? 0 : kURLBarHeight;
    NSRect frame = NSMakeRect(0, 0, content_size.width(),
                              content_size.height() + toolbar_height);
    [window().GetNativeNSWindow().contentView setFrame:frame];
    return;
  }
  NSView* web_view = web_contents_->GetNativeView().GetNativeNSView();
  NSRect frame = NSMakeRect(0, 0, content_size.width(), content_size.height());
  [web_view setFrame:frame];
}

void Shell::PlatformResizeSubViews() {
  // Not needed; subviews are bound.
}

void Shell::PlatformSetTitle(const base::string16& title) {
  if (headless_)
    return;

  NSString* title_string = base::SysUTF16ToNSString(title);
  [window_.GetNativeNSWindow() setTitle:title_string];
}

void Shell::Close() {
  if (headless_)
    delete this;
  else
    [window_.GetNativeNSWindow() performClose:nil];
}

void Shell::ActionPerformed(int control) {
  switch (control) {
    case IDC_NAV_BACK:
      GoBackOrForward(-1);
      break;
    case IDC_NAV_FORWARD:
      GoBackOrForward(1);
      break;
    case IDC_NAV_RELOAD:
      Reload();
      break;
    case IDC_NAV_STOP:
      Stop();
      break;
  }
}

void Shell::URLEntered(const std::string& url_string) {
  if (!url_string.empty()) {
    GURL url(url_string);
    if (!url.has_scheme())
      url = GURL("http://" + url_string);
    LoadURL(url);
  }
}

bool Shell::HandleKeyboardEvent(WebContents* source,
                                const NativeWebKeyboardEvent& event) {
  if (event.skip_in_browser || headless_ || hide_toolbar_)
    return false;

  // The event handling to get this strictly right is a tangle; cheat here a bit
  // by just letting the menus have a chance at it.
  if ([event.os_event type] == NSKeyDown) {
    if (([event.os_event modifierFlags] & NSCommandKeyMask) &&
        [[event.os_event characters] isEqual:@"l"]) {
      [window_.GetNativeNSWindow() makeFirstResponder:url_edit_view_];
      return true;
    }

    [[NSApp mainMenu] performKeyEquivalent:event.os_event];
    return true;
  }
  return false;
}

}  // namespace content
