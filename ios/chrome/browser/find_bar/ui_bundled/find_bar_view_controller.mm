// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_view.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"

@interface FindBarViewController ()

@property(nonatomic, assign) BOOL darkAppearance;

@end

@implementation FindBarViewController

- (instancetype)initWithDarkAppearance:(BOOL)darkAppearance {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    self.overrideUserInterfaceStyle = darkAppearance
                                          ? UIUserInterfaceStyleDark
                                          : UIUserInterfaceStyleUnspecified;
  }
  return self;
}

#pragma mark - UIView

- (void)loadView {
  self.view = [[FindBarView alloc] init];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
}

#pragma mark - Property Getters

- (FindBarView*)findBarView {
  return base::apple::ObjCCastStrict<FindBarView>(self.view);
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self.delegate dismiss];
}

@end
