// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "remoting/host/continue_window.h"

#import <Cocoa/Cocoa.h>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

// Handles the ContinueWindow.
@interface ContinueWindowMacController : NSObject {
 @private
  base::scoped_nsobject<NSMutableArray> _shades;
  base::scoped_nsobject<NSAlert> _continue_alert;
  raw_ptr<remoting::ContinueWindow> _continue_window;
}

- (instancetype)initWithWindow:(remoting::ContinueWindow*)continue_window;
- (void)show;
- (void)hide;
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

 private:
  base::scoped_nsobject<ContinueWindowMacController> controller_;
};

ContinueWindowMac::ContinueWindowMac() {
}

ContinueWindowMac::~ContinueWindowMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (controller_) {
    HideUi();
  }
}

void ContinueWindowMac::ShowUi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  @autoreleasepool {
    controller_.reset(
        [[ContinueWindowMacController alloc] initWithWindow:this]);
    [controller_ show];
  }
}

void ContinueWindowMac::HideUi() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  @autoreleasepool {
    [controller_ hide];
    controller_.reset();
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
  NSArray* screens = [NSScreen screens];
  _shades.reset([[NSMutableArray alloc] initWithCapacity:[screens count]]);
  for (NSScreen *screen in screens) {
    NSWindow* shade =
        [[[NSWindow alloc] initWithContentRect:[screen frame]
                                     styleMask:NSWindowStyleMaskBorderless
                                       backing:NSBackingStoreBuffered
                                         defer:NO
                                        screen:screen] autorelease];
    [shade setReleasedWhenClosed:NO];
    [shade setAlphaValue:0.8];
    [shade setOpaque:NO];
    [shade setBackgroundColor:[NSColor blackColor]];
    // Raise the window shade above just about everything else.
    // Leave the dock and menu bar exposed so the user has some basic level
    // of control (like they can quit Chromium).
    [shade setLevel:NSModalPanelWindowLevel - 1];
    [shade orderFront:nil];
    [_shades addObject:shade];
  }

  // Create alert.
  _continue_alert.reset([[NSAlert alloc] init]);
  [_continue_alert setMessageText:l10n_util::GetNSString(IDS_CONTINUE_PROMPT)];

  NSButton* continue_button =
      [_continue_alert addButtonWithTitle:l10n_util::GetNSString(
          IDS_CONTINUE_BUTTON)];
  [continue_button setAction:@selector(onContinue:)];
  [continue_button setTarget:self];

  NSButton* cancel_button =
      [_continue_alert addButtonWithTitle:l10n_util::GetNSString(
          IDS_STOP_SHARING_BUTTON)];
  [cancel_button setAction:@selector(onCancel:)];
  [cancel_button setTarget:self];

  NSBundle *bundle = [NSBundle bundleForClass:[self class]];
  NSString *imagePath = [bundle pathForResource:@"chromoting128" ofType:@"png"];
  base::scoped_nsobject<NSImage> image(
      [[NSImage alloc] initByReferencingFile:imagePath]);
  [_continue_alert setIcon:image];
  [_continue_alert layout];

  // Force alert to be at the proper level and location.
  NSWindow* continue_window = [_continue_alert window];
  [continue_window center];
  [continue_window setLevel:NSModalPanelWindowLevel];
  [continue_window orderWindow:NSWindowAbove
                    relativeTo:[[_shades lastObject] windowNumber]];
  [continue_window makeKeyWindow];
}

- (void)hide {
  // Remove window shade.
  for (NSWindow* window in _shades.get()) {
    [window close];
  }
  _shades.reset();
  if (_continue_alert) {
    [[_continue_alert window] close];
    _continue_alert.reset();
  }
}

- (void)onCancel:(id)sender {
  [self hide];
  _continue_window->DisconnectSession();
}

- (void)onContinue:(id)sender {
  [self hide];
  _continue_window->ContinueSession();
}

@end
