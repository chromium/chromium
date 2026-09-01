// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/disconnect_window_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr int kMaximumConnectedNameWidthInPixels = 600;

enum class WindowAnchor {
  kBottom,
  kTop,
};

// Remembers the last selected anchor position across dialog instances.
WindowAnchor g_current_anchor = WindowAnchor::kBottom;

// The amount of time to wait before allowing another position toggle.
constexpr base::TimeDelta kToggleCooldown = base::Seconds(3);

// Margins from screen edges to ensure the dialog is not obscured by the menu
// bar at the top or an auto-hiding Dock at the bottom.
constexpr CGFloat kTopMargin = 40.0;
constexpr CGFloat kBottomMargin = 80.0;

bool IsDarkMode() {
  NSAppearanceName appearance =
      [NSApp.effectiveAppearance bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua
      ]];
  return [appearance isEqual:NSAppearanceNameDarkAqua];
}

}  // namespace

@interface DisconnectWindowController ()
- (BOOL)isRToL;
- (void)hide;
- (void)updateToggleButtonText;
- (void)setDialogPosition;
- (void)onScreenParametersChanged:(NSNotification*)notification;
- (void)onCooldownExpired;
- (IBAction)toggleAlignment:(id)sender;
@property(nonatomic, strong) NSButton* toggleButton;
@property(nonatomic, strong) NSTextField* connectedToField;
@property(nonatomic, strong) NSButton* disconnectButton;
@end

namespace remoting {

class DisconnectWindowMac : public HostWindow {
 public:
  DisconnectWindowMac();

  DisconnectWindowMac(const DisconnectWindowMac&) = delete;
  DisconnectWindowMac& operator=(const DisconnectWindowMac&) = delete;

  ~DisconnectWindowMac() override;

  // HostWindow overrides.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

 private:
  DisconnectWindowController* __strong window_controller_;
};

DisconnectWindowMac::DisconnectWindowMac() = default;

DisconnectWindowMac::~DisconnectWindowMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [window_controller_ hide];
  window_controller_ = nil;
}

void DisconnectWindowMac::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_session_control);
  DCHECK(window_controller_ == nil);

  // Create the window.
  base::OnceClosure disconnect_callback = base::BindOnce(
      &ClientSessionControl::DisconnectSession, client_session_control,
      ErrorCode::OK, "Disconnect button was clicked.", FROM_HERE);
  std::string client_jid = client_session_control->client_jid();
  std::string username = client_jid.substr(0, client_jid.find('/'));

  NSRect frame = NSMakeRect(0, 0, 466, 40);
  DisconnectWindow* window =
      [[DisconnectWindow alloc] initWithContentRect:frame
                                          styleMask:NSWindowStyleMaskBorderless
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
  window.releasedWhenClosed = NO;
  window_controller_ = [[DisconnectWindowController alloc]
      initWithCallback:std::move(disconnect_callback)
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

@implementation DisconnectWindowController {
  base::OnceClosure _disconnect_callback;
  std::u16string _username;
  base::OneShotTimer _cooldown_timer;
}

@synthesize toggleButton = _toggleButton;
@synthesize connectedToField = _connectedToField;
@synthesize disconnectButton = _disconnectButton;

- (instancetype)initWithCallback:(base::OnceClosure)disconnect_callback
                        username:(const std::string&)username
                          window:(NSWindow*)window {
  self = [super initWithWindow:window];
  if (self) {
    _disconnect_callback = std::move(disconnect_callback);
    _username = base::UTF8ToUTF16(username);
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(onScreenParametersChanged:)
               name:NSApplicationDidChangeScreenParametersNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (IBAction)stopSharing:(id)sender {
  if (_disconnect_callback) {
    std::move(_disconnect_callback).Run();
  }
}

- (IBAction)toggleAlignment:(id)sender {
  g_current_anchor = (g_current_anchor == WindowAnchor::kBottom)
                         ? WindowAnchor::kTop
                         : WindowAnchor::kBottom;
  [self updateToggleButtonText];
  self.toggleButton.enabled = NO;
  __weak __typeof__(self) weakSelf = self;
  _cooldown_timer.Start(FROM_HERE, kToggleCooldown, base::BindOnce(^{
                          [weakSelf onCooldownExpired];
                        }));
  [self setDialogPosition];
}

- (void)onCooldownExpired {
  self.toggleButton.enabled = YES;
}

- (void)updateToggleButtonText {
  self.toggleButton.title =
      (g_current_anchor == WindowAnchor::kBottom) ? @"▲" : @"▼";
  int string_id = (g_current_anchor == WindowAnchor::kBottom)
                      ? IDS_MOVE_TO_TOP_BUTTON
                      : IDS_MOVE_TO_BOTTOM_BUTTON;
  NSString* tooltip_text = l10n_util::GetNSString(string_id);
  self.toggleButton.toolTip = tooltip_text;
  self.toggleButton.accessibilityLabel = tooltip_text;
}

- (BOOL)isRToL {
  return base::i18n::IsRTL();
}

- (void)hide {
  [NSNotificationCenter.defaultCenter removeObserver:self];
  _cooldown_timer.Stop();
  _disconnect_callback.Reset();
  [self close];
}

- (void)initializeWindow {
  self.window.contentView =
      [[DisconnectView alloc] initWithFrame:self.window.contentView.frame];

  self.toggleButton =
      [[NSButton alloc] initWithFrame:NSMakeRect(12, 9, 22, 22)];
  self.toggleButton.buttonType = NSButtonTypeMomentaryPushIn;
  self.toggleButton.bezelStyle = NSBezelStyleFlexiblePush;
  self.toggleButton.font = [NSFont systemFontOfSize:11];
  self.toggleButton.action = @selector(toggleAlignment:);
  self.toggleButton.target = self;
  [self updateToggleButtonText];
  [self.window.contentView addSubview:self.toggleButton];

  self.connectedToField =
      [[NSTextField alloc] initWithFrame:NSMakeRect(40, 13, 240, 14)];
  self.connectedToField.drawsBackground = NO;
  self.connectedToField.bezeled = NO;
  self.connectedToField.editable = NO;
  self.connectedToField.font = [NSFont systemFontOfSize:11];
  [self.window.contentView addSubview:self.connectedToField];

  self.disconnectButton =
      [[NSButton alloc] initWithFrame:NSMakeRect(271, 9, 182, 22)];
  self.disconnectButton.buttonType = NSButtonTypeMomentaryPushIn;
  self.disconnectButton.bezelStyle = NSBezelStyleFlexiblePush;
  self.disconnectButton.font = [NSFont systemFontOfSize:11];
  self.disconnectButton.action = @selector(stopSharing:);
  self.disconnectButton.target = self;
  [self.window.contentView addSubview:self.disconnectButton];

  self.connectedToField.stringValue =
      l10n_util::GetNSStringF(IDS_MESSAGE_SHARED, _username);
  self.disconnectButton.title = l10n_util::GetNSString(IDS_STOP_SHARING_BUTTON);

  // Resize the window dynamically based on the content.
  [self.connectedToField sizeToFit];
  NSRect connectedToFrame = self.connectedToField.frame;
  CGFloat newConnectedWidth = NSWidth(connectedToFrame);

  // Set a max width for the connected to text field.
  if (newConnectedWidth > kMaximumConnectedNameWidthInPixels) {
    newConnectedWidth = kMaximumConnectedNameWidthInPixels;
    connectedToFrame.size.width = newConnectedWidth;
    self.connectedToField.frame = connectedToFrame;
  }

  [self.disconnectButton sizeToFit];
  NSRect disconnectFrame = self.disconnectButton.frame;
  CGFloat newDisconnectWidth = NSWidth(disconnectFrame);

  // Align vertical centers.
  CGFloat contentViewHeight = NSHeight(self.window.contentView.frame);
  NSRect toggleFrame = self.toggleButton.frame;
  toggleFrame.origin.y = (contentViewHeight - NSHeight(toggleFrame)) / 2;
  connectedToFrame.origin.y =
      (contentViewHeight - NSHeight(connectedToFrame)) / 2;
  disconnectFrame.origin.y =
      (contentViewHeight - NSHeight(disconnectFrame)) / 2;

  const CGFloat kMargin = 12.0;
  const CGFloat kToggleGap = 6.0;
  const CGFloat kButtonGap = 12.0;

  // Calculate total window width.
  CGFloat totalWidth = kMargin + NSWidth(toggleFrame) + kToggleGap +
                       newConnectedWidth + kButtonGap + newDisconnectWidth +
                       kMargin;

  NSRect windowFrame = self.window.frame;
  windowFrame.size.width = totalWidth;
  [self.window setFrame:windowFrame display:NO];

  if ([self isRToL]) {
    // Handle right-to-left layout: [Stop Sharing] [Message] [Toggle]
    disconnectFrame.origin.x = kMargin;
    connectedToFrame.origin.x = NSMaxX(disconnectFrame) + kButtonGap;
    toggleFrame.origin.x = NSMaxX(connectedToFrame) + kToggleGap;
  } else {
    // Handle left-to-right layout: [Toggle] [Message] [Stop Sharing]
    toggleFrame.origin.x = kMargin;
    connectedToFrame.origin.x = NSMaxX(toggleFrame) + kToggleGap;
    disconnectFrame.origin.x = NSMaxX(connectedToFrame) + kButtonGap;
  }

  self.toggleButton.frame = toggleFrame;
  self.connectedToField.frame = connectedToFrame;
  self.disconnectButton.frame = disconnectFrame;

  [self setDialogPosition];
}

- (void)setDialogPosition {
  NSRect screenRect = NSScreen.mainScreen.frame;
  NSRect windowRect = self.window.frame;
  CGFloat x =
      NSMinX(screenRect) + (NSWidth(screenRect) - NSWidth(windowRect)) / 2;
  CGFloat y = (g_current_anchor == WindowAnchor::kTop)
                  ? NSMaxY(screenRect) - NSHeight(windowRect) - kTopMargin
                  : NSMinY(screenRect) + kBottomMargin;
  [self.window setFrameOrigin:NSMakePoint(x, y)];
}

- (void)onScreenParametersChanged:(NSNotification*)notification {
  [self setDialogPosition];
}

- (void)windowWillClose:(NSNotification*)notification {
  [self stopSharing:self];
}

@end

@implementation DisconnectWindow

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)aStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)flag {
  // Pass NSWindowStyleMaskBorderless for the styleMask to remove the title bar.
  self = [super initWithContentRect:contentRect
                          styleMask:NSWindowStyleMaskBorderless
                            backing:bufferingType
                              defer:flag];

  if (self) {
    // Set window to be clear and non-opaque so we can see through it.
    self.backgroundColor = NSColor.clearColor;
    self.opaque = NO;

    // Pull the window up to Status Level so that it always displays.
    [self setLevel:NSStatusWindowLevel];

    // Allow the window to join all spaces, appear with full-screen windows, and
    // remain stationary during Mission Control / Exposé.
    self.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                              NSWindowCollectionBehaviorFullScreenAuxiliary |
                              NSWindowCollectionBehaviorStationary;
  }
  return self;
}

@end

@implementation DisconnectView

- (void)drawRect:(NSRect)rect {
  // All magic numbers taken from screen shots provided by UX.
  NSRect bounds = NSInsetRect(self.bounds, 1, 1);

  NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                       xRadius:5
                                                       yRadius:5];
  NSColor* bgColor;
  NSColor* frameColor;
  if (IsDarkMode()) {
    bgColor = [NSColor colorWithCalibratedWhite:0.2 alpha:1.0];
    frameColor = [NSColor colorWithCalibratedWhite:0.91 alpha:1.0];
  } else {
    bgColor = [NSColor colorWithCalibratedWhite:0.91 alpha:1.0];
    frameColor = [NSColor colorWithCalibratedRed:0.13
                                           green:0.69
                                            blue:0.11
                                           alpha:1.0];
  }
  [bgColor setFill];
  [path fill];
  [path setLineWidth:4];
  [frameColor setStroke];
  [path stroke];
}

@end
