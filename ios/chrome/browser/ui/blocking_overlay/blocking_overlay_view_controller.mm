// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/blocking_overlay/blocking_overlay_view_controller.h"

#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Max width for explanatory text.
const CGFloat kMaxTextWidth = 275.0f;
// Vertical space between text and button.
const CGFloat kButtonSpacing = 20.0f;

}  // namespace

@implementation BlockingOverlayViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Set up a blurred background.
  UIBlurEffect* effect = nil;
  // SystemXYZMaterial effect styles are iOS 13+, but so is multiwindow.
  if (@available(iOS 13, *)) {
    effect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThinMaterial];
  }

  UIVisualEffectView* backgroundBlurEffect =
      [[UIVisualEffectView alloc] initWithEffect:effect];
  backgroundBlurEffect.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:backgroundBlurEffect];
  AddSameConstraints(self.view, backgroundBlurEffect);

  UILabel* label = [[UILabel alloc] init];
  label.text =
      l10n_util::GetNSString(IDS_IOS_UI_BLOCKED_USE_OTHER_WINDOW_DESCRIPTION);
  label.textColor = [UIColor colorNamed:kGrey800Color];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.textAlignment = NSTextAlignmentCenter;
  label.numberOfLines = 0;

  [self.view addSubview:label];
  AddSameCenterConstraints(label, self.view);
  [label.widthAnchor constraintLessThanOrEqualToConstant:kMaxTextWidth].active =
      YES;
  label.translatesAutoresizingMaskIntoConstraints = NO;

  UIButton* button = [[UIButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button setTitle:l10n_util::GetNSString(
                       IDS_IOS_UI_BLOCKED_USE_OTHER_WINDOW_SWITCH_WINDOW_ACTION)
          forState:UIControlStateNormal];
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [button addTarget:self
                action:@selector(buttonPressed:)
      forControlEvents:UIControlEventTouchUpInside];

  [self.view addSubview:button];
  [NSLayoutConstraint activateConstraints:@[
    [button.leadingAnchor constraintEqualToAnchor:label.leadingAnchor],
    [button.trailingAnchor constraintEqualToAnchor:label.trailingAnchor],
    [button.topAnchor constraintEqualToAnchor:label.bottomAnchor
                                     constant:kButtonSpacing],
  ]];
}

- (void)buttonPressed:(id)sender {
  if (@available(iOS 13, *)) {
    [self.blockingSceneCommandHandler
        activateBlockingScene:self.view.window.windowScene];
  }
}

@end
