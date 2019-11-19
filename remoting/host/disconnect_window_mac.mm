// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/disconnect_window_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface DisconnectWindowController()
- (BOOL)isRToL;
- (void)Hide;
@property(nonatomic, retain) NSTextField* connectedToField;
@property(nonatomic, retain) NSButton* disconnectButton;
@end

const int kMaximumConnectedNameWidthInPixels = 600;

namespace {

bool IsDarkMode() {
  if (@available(macOS 10.14, *)) {
    NSAppearanceName appearance =
        [[NSApp effectiveAppearance] bestMatchFromAppearancesWithNames:@[
          NSAppearanceNameAqua, NSAppearanceNameDarkAqua
        ]];
    return [appearance isEqual:NSAppearanceNameDarkAqua];
  }
  return false;
}

}  // namespace

namespace remoting {

class DisconnectWindowMac : public HostWindow {
 public:
  DisconnectWindowMac();
  ~DisconnectWindowMac() override;

  // HostWindow overrides.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

 private:
  DisconnectWindowController* window_controller_;

  DISALLOW_COPY_AND_ASSIGN(DisconnectWindowMac);
};

DisconnectWindowMac::DisconnectWindowMac()
    : window_controller_(nil) {
}

DisconnectWindowMac::~DisconnectWindowMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // DisconnectWindowController is responsible for releasing itself in its
  // windowWillClose: method.
  [window_controller_ Hide];
  window_controller_ = nil;
}

void DisconnectWindowMac::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_session_control);
  DCHECK(window_controller_ == nil);

  // Create the window.
  base::Closure disconnect_callback =
      base::Bind(&ClientSessionControl::DisconnectSession,
                 client_session_control, protocol::OK);
  std::string client_jid = client_session_control->client_jid();
  std::string username = client_jid.substr(0, client_jid.find('/'));

  NSRect frame = NSMakeRect(0, 0, 466, 40);
  DisconnectWindow* window =
      [[[DisconnectWindow alloc] initWithContentRect:frame
                                           styleMask:NSBorderlessWindowMask
                                             backing:NSBackingStoreBuffered
                                               defer:NO] autorelease];
  window_controller_ =
      [[DisconnectWindowController alloc] initWithCallback:disconnect_callback
                                                  username:username
                                                    window:window];
  [window_controller_ initializeWindow];
  [window_controller_ showWindow:nil];
}

// static
std::unique_ptr<HostWindow> HostWindow::CreateDisconnectWindow() {
  return std::make_unique<DisconnectWindowMac>();
}

}  // namespace remoting

@implementation DisconnectWindowController
@synthesize connectedToField = connectedToField_;
@synthesize disconnectButton = disconnectButton_;

- (id)initWithCallback:(const base::Closure&)disconnect_callback
              username:(const std::string&)username
                window:(NSWindow*)window {
  self = [super initWithWindow:(NSWindow*)window];
  if (self) {
    disconnect_callback_ = disconnect_callback;
    username_ = base::UTF8ToUTF16(username);
  }
  return self;
}

- (void)dealloc {
  [super dealloc];
}

- (IBAction)stopSharing:(id)sender {
  if (!disconnect_callback_.is_null()) {
    disconnect_callback_.Run();
  }
}

- (BOOL)isRToL {
  return base::i18n::IsRTL();
}

- (void)Hide {
  disconnect_callback_.Reset();
  [self close];
}

- (void)initializeWindow {
  self.window.contentView = [[[DisconnectView alloc]
      initWithFrame:self.window.contentView.frame] autorelease];

  self.connectedToField = [[[NSTextField alloc]
      initWithFrame:NSMakeRect(26, 13, 240, 14)] autorelease];
  self.connectedToField.drawsBackground = NO;
  self.connectedToField.bezeled = NO;
  self.connectedToField.editable = NO;
  self.connectedToField.font = [NSFont systemFontOfSize:11];
  [self.window.contentView addSubview:self.connectedToField];

  self.disconnectButton = [[[NSButton alloc]
      initWithFrame:NSMakeRect(271, 9, 182, 22)] autorelease];
  self.disconnectButton.buttonType = NSButtonTypeMomentaryPushIn;
  self.disconnectButton.bezelStyle = NSBezelStyleRegularSquare;
  self.disconnectButton.font = [NSFont systemFontOfSize:11];
  self.disconnectButton.action = @selector(stopSharing:);
  self.disconnectButton.target = self;
  [self.window.contentView addSubview:self.disconnectButton];

  [connectedToField_ setStringValue:l10n_util::GetNSStringF(IDS_MESSAGE_SHARED,
                                                            username_)];
  [disconnectButton_ setTitle:l10n_util::GetNSString(IDS_STOP_SHARING_BUTTON)];

  // Resize the window dynamically based on the content.
  CGFloat oldConnectedWidth = NSWidth([connectedToField_ bounds]);
  [connectedToField_ sizeToFit];
  NSRect connectedToFrame = [connectedToField_ frame];
  CGFloat newConnectedWidth = NSWidth(connectedToFrame);

  // Set a max width for the connected to text field.
  if (newConnectedWidth > kMaximumConnectedNameWidthInPixels) {
    newConnectedWidth = kMaximumConnectedNameWidthInPixels;
    connectedToFrame.size.width = newConnectedWidth;
    [connectedToField_ setFrame:connectedToFrame];
  }

  CGFloat oldDisconnectWidth = NSWidth([disconnectButton_ bounds]);
  [disconnectButton_ sizeToFit];
  NSRect disconnectFrame = [disconnectButton_ frame];
  CGFloat newDisconnectWidth = NSWidth(disconnectFrame);

  // Move the disconnect button appropriately.
  disconnectFrame.origin.x += newConnectedWidth - oldConnectedWidth;
  disconnectFrame.origin.y =
      (NSHeight(self.window.contentView.frame) - NSHeight(disconnectFrame)) / 2;
  [disconnectButton_ setFrame:disconnectFrame];

  // Then resize the window appropriately
  NSWindow *window = [self window];
  NSRect windowFrame = [window frame];
  windowFrame.size.width += (newConnectedWidth - oldConnectedWidth +
                             newDisconnectWidth - oldDisconnectWidth);
  [window setFrame:windowFrame display:NO];

  if ([self isRToL]) {
    // Handle right to left case
    CGFloat buttonInset = NSWidth(windowFrame) - NSMaxX(disconnectFrame);
    CGFloat buttonTextSpacing
        = NSMinX(disconnectFrame) - NSMaxX(connectedToFrame);
    disconnectFrame.origin.x = buttonInset;
    connectedToFrame.origin.x = NSMaxX(disconnectFrame) + buttonTextSpacing;
    [connectedToField_ setFrame:connectedToFrame];
    [disconnectButton_ setFrame:disconnectFrame];
  }

  // Center the window at the bottom of the screen, above the dock (if present).
  NSRect desktopRect = [[NSScreen mainScreen] visibleFrame];
  NSRect windowRect = [[self window] frame];
  CGFloat x = (NSWidth(desktopRect) - NSWidth(windowRect)) / 2;
  CGFloat y = NSMinY(desktopRect);
  [[self window] setFrameOrigin:NSMakePoint(x, y)];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self stopSharing:self];
  [self autorelease];
}

@end


@interface DisconnectWindow()
- (BOOL)isRToL;
@end

@implementation DisconnectWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)bufferingType
                  defer:(BOOL)flag {
  // Pass NSBorderlessWindowMask for the styleMask to remove the title bar.
  self = [super initWithContentRect:contentRect
                          styleMask:NSBorderlessWindowMask
                            backing:bufferingType
                              defer:flag];

  if (self) {
    // Set window to be clear and non-opaque so we can see through it.
    [self setBackgroundColor:[NSColor clearColor]];
    [self setOpaque:NO];
    [self setMovableByWindowBackground:YES];

    // Pull the window up to Status Level so that it always displays.
    [self setLevel:NSStatusWindowLevel];
  }
  return self;
}

- (BOOL)isRToL {
  DCHECK([[self windowController] respondsToSelector:@selector(isRToL)]);
  return [[self windowController] isRToL];
}

@end


@interface DisconnectView()
- (BOOL)isRToL;
@end

@implementation DisconnectView

- (BOOL)isRToL {
  DCHECK([[self window] isKindOfClass:[DisconnectWindow class]]);
  return [static_cast<DisconnectWindow*>([self window]) isRToL];
}

- (void)drawRect:(NSRect)rect {
  // All magic numbers taken from screen shots provided by UX.
  NSRect bounds = NSInsetRect([self bounds], 1, 1);

  NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                       xRadius:5
                                                       yRadius:5];
  NSColor* bgColor;
  NSColor* frameColor;
  NSColor* lineColor;
  NSColor* lineShadowColor;
  if (IsDarkMode()) {
    bgColor = [NSColor colorWithCalibratedWhite:0.2 alpha:1.0];
    frameColor = [NSColor colorWithCalibratedWhite:0.91 alpha:1.0];
    lineColor = [NSColor colorWithCalibratedWhite:0.91 alpha:1.0];
    lineShadowColor = [NSColor colorWithCalibratedWhite:0.32 alpha:1.0];
  } else {
    bgColor = [NSColor colorWithCalibratedWhite:0.91 alpha:1.0];
    frameColor = [NSColor colorWithCalibratedRed:0.13
                                           green:0.69
                                            blue:0.11
                                           alpha:1.0];
    lineColor = [NSColor colorWithCalibratedWhite:0.70 alpha:1.0];
    lineShadowColor = [NSColor colorWithCalibratedWhite:0.97 alpha:1.0];
  }
  [bgColor setFill];
  [path fill];
  [path setLineWidth:4];
  [frameColor setStroke];
  [path stroke];


  // Draw drag handle on proper side
  const CGFloat kHeight = 21.0;
  const CGFloat kBaseInset = 12.0;
  const CGFloat kDragHandleWidth = 5.0;

  // Turn off aliasing so it's nice and crisp.
  NSGraphicsContext *context = [NSGraphicsContext currentContext];
  BOOL alias = [context shouldAntialias];
  [context setShouldAntialias:NO];

  // Handle bidirectional locales properly.
  CGFloat inset = [self isRToL] ? NSMaxX(bounds) - kBaseInset - kDragHandleWidth
                                : kBaseInset;

  NSPoint top = NSMakePoint(inset, NSMidY(bounds) - kHeight / 2.0);
  NSPoint bottom = NSMakePoint(inset, top.y + kHeight);

  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [lineColor setStroke];
  [path stroke];

  top.x += 1;
  bottom.x += 1;
  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [lineShadowColor setStroke];
  [path stroke];

  top.x += 2;
  bottom.x += 2;
  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [lineColor setStroke];
  [path stroke];

  top.x += 1;
  bottom.x += 1;
  path = [NSBezierPath bezierPath];
  [path moveToPoint:top];
  [path lineToPoint:bottom];
  [lineShadowColor setStroke];
  [path stroke];

  [context setShouldAntialias:alias];
}

@end
