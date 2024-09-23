// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/tos/tos_view_controller.h"

#import <WebKit/WebKit.h>

#import "base/check.h"
#import "ios/chrome/browser/shared/public/commands/tos_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface TOSViewController ()

@property(nonatomic, strong) UIView* TOSView;
@property(nonatomic, weak) id<TOSCommands> handler;

@end

@implementation TOSViewController

- (instancetype)initWithContentView:(UIView*)TOSView
                            handler:(id<TOSCommands>)handler {
  DCHECK(TOSView);
  self = [super init];
  if (self) {
    _TOSView = TOSView;
    _handler = handler;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self configureNavigationBar];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.TOSView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.TOSView];

  [NSLayoutConstraint activateConstraints:@[
    [self.TOSView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.TOSView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.TOSView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.TOSView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ]];
}

#pragma mark - Button events

// Called by the Done button from the navigation bar.
- (void)close {
  [self.handler closeTOSPage];
}

#pragma mark - Private

// Configures the top navigation bar (colors, texts, buttons, etc.)
- (void)configureNavigationBar {
  self.navigationController.navigationBar.translucent = NO;
  self.navigationController.navigationBar.barTintColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.navigationController.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  self.title = l10n_util::GetNSString(IDS_IOS_FIRSTRUN_TERMS_TITLE);

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(close)];
  self.navigationItem.rightBarButtonItem = doneButton;
}

@end
