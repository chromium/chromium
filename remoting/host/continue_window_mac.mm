// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

// Handles the ContinueWindow.
@interface ContinueWindowMacController : NSObject <NSWindowDelegate> {
 @private
  NSMutableArray<NSWindow*>* __strong _shades;
  NSAlert* __strong _continue_alert;
  NSButton* __weak _cancel_button;
  NSButton* __weak _continue_button;
  raw_ptr<remoting::ContinueWindow> _continue_window;
}

- (instancetype)initWithWindow:(remoting::ContinueWindow*)continue_window;
- (void)show;
- (void)hide;
- (void)cancelOperation:(id)sender;
- (void)setButtonsEnabled:(BOOL)enabled;
- (void)onCancel:(id)sender;
- (void)onContinue:(id)sender;
@end

namespace remoting {

// A bridge between C++ and ObjC implementations of ContinueWindow.
// Everything important occurs in ContinueWindowMacController.
class ContinueWindowMac : public ContinueWindow {
 public:
  ContinueWindowMac();

  ContinueWindowMac(const ContinueWindowMac&) = delete;
  ContinueWindowMac& operator=(const ContinueWindowMac&) = delete;

  ~ContinueWindowMac() override;

 protected:
  // ContinueWindow overrides.
  void ShowUi() override;
  void HideUi() override;
  void SetButtonsEnabled(bool enabled) override;

 private:
  ContinueWindowMacController* __strong controller_;
};

ContinueWindowMac::ContinueWindowMac() = default;

ContinueWindowMac::~ContinueWindowMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (controller_) {
    HideUi();
  }
}

void ContinueWindowMac::ShowUi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  @autoreleasepool {
    controller_ = [[ContinueWindowMacController alloc] initWithWindow:this];
    [controller_ show];
  }
}

void ContinueWindowMac::HideUi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  @autoreleasepool {
    [controller_ hide];
    controller_ = nil;
  }
}

void ContinueWindowMac::SetButtonsEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (controller_) {
    [controller_ setButtonsEnabled:enabled ? YES : NO];
  }
}

// static
std::unique_ptr<HostWindow> HostWindow::CreateContinueWindow() {
  return std::make_unique<ContinueWindowMac>();
}

}  // namespace remoting

@implementation ContinueWindowMacController

- (instancetype)initWithWindow:(remoting::ContinueWindow*)continue_window {
  if ((self = [super init])) {
    _continue_window = continue_window;
  }
  return self;
}

- (void)show {
  // Generate window shade
  NSArray* screens = NSScreen.screens;
  _shades = [[NSMutableArray alloc] initWithCapacity:screens.count];
  for (NSScreen* screen in screens) {
    NSWindow* shade =
        [[NSWindow alloc] initWithContentRect:screen.frame
                                    styleMask:NSWindowStyleMaskBorderless
                                      backing:NSBackingStoreBuffered
                                        defer:NO
                                       screen:screen];
    shade.releasedWhenClosed = NO;
    shade.alphaValue = 0.8;
    shade.opaque = NO;
    shade.backgroundColor = NSColor.blackColor;
    // Raise the window shade above just about everything else.
    // Leave the dock and menu bar exposed so the user has some basic level
    // of control (like they can quit Chromium).
    shade.level = NSModalPanelWindowLevel - 1;
    shade.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                               NSWindowCollectionBehaviorFullScreenAuxiliary |
                               NSWindowCollectionBehaviorStationary;
    [shade orderFront:nil];
    [_shades addObject:shade];
  }

  // Create alert.
  _continue_alert = [[NSAlert alloc] init];
  _continue_alert.messageText = l10n_util::GetNSString(IDS_CONTINUE_PROMPT);

  _cancel_button = [_continue_alert
      addButtonWithTitle:l10n_util::GetNSString(IDS_STOP_SHARING_BUTTON)];
  _cancel_button.action = @selector(onCancel:);
  _cancel_button.target = self;
  _cancel_button.enabled = NO;

  _continue_button = [_continue_alert
      addButtonWithTitle:l10n_util::GetNSString(IDS_CONTINUE_BUTTON)];
  _continue_button.action = @selector(onContinue:);
  _continue_button.target = self;
  _continue_button.keyEquivalent = @"";
  _continue_button.enabled = NO;

  NSBundle* bundle = [NSBundle bundleForClass:[self class]];
  NSString* imagePath = [bundle pathForResource:@"chromoting128" ofType:@"png"];
  NSImage* image = [[NSImage alloc] initByReferencingFile:imagePath];
  _continue_alert.icon = image;
  [_continue_alert layout];

  // Force alert to be at the proper level and location.
  NSWindow* continue_window = _continue_alert.window;
  continue_window.delegate = self;
  [continue_window center];
  continue_window.level = NSModalPanelWindowLevel;
  continue_window.collectionBehavior =
      NSWindowCollectionBehaviorCanJoinAllSpaces |
      NSWindowCollectionBehaviorFullScreenAuxiliary |
      NSWindowCollectionBehaviorStationary;
  [continue_window orderWindow:NSWindowAbove
                    relativeTo:_shades.lastObject.windowNumber];
  [continue_window makeKeyWindow];
}

- (void)hide {
  // Remove window shade.
  for (NSWindow* window in _shades) {
    [window close];
  }
  _shades = nil;
  if (_continue_alert) {
    _continue_alert.window.delegate = nil;
    [_continue_alert.window close];
    _continue_alert = nil;
  }
  _cancel_button = nil;
  _continue_button = nil;
}

- (void)setButtonsEnabled:(BOOL)enabled {
  _cancel_button.enabled = enabled;
  _continue_button.enabled = enabled;
}

- (void)cancelOperation:(id)sender {
  [self onCancel:sender];
}

- (void)onCancel:(id)sender {
  if (!_cancel_button.enabled) {
    return;
  }

  [self hide];
  _continue_window->DisconnectSession();
}

- (void)onContinue:(id)sender {
  if (!_continue_button.enabled) {
    return;
  }

  [self hide];
  _continue_window->ContinueSession();
}

@end
