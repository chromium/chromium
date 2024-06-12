// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/blocking_overlay/ui_bundled/blocking_overlay_view_controller.h"

#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"

namespace {
// Max width for explanatory text.
const CGFloat kMaxTextWidth = 275.0f;
// Vertical space between text and button.
const CGFloat kButtonSpacing = 20.0f;

}  // namespace

// This view controller is used in Safe Mode. This means everything used here
// must not require any advanced initialization that only happens after safe
// mode.
@implementation BlockingOverlayViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Set up a blurred background.
  UIBlurEffect* effect = nil;
  // SystemXYZMaterial effect styles are iOS 13+, but so is multiwindow.
  effect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThinMaterial];

  UIVisualEffectView* backgroundBlurEffect =
      [[UIVisualEffectView alloc] initWithEffect:effect];
  backgroundBlurEffect.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:backgroundBlurEffect];
  AddSameConstraints(self.view, backgroundBlurEffect);

  UILabel* label = [[UILabel alloc] init];
  // Here and everywhere, use NSLocalizedString because the usual localization
  // method with l10n_util::GetNSString() requires initialization that happens
  // after safe mode completes.
  label.text = NSLocalizedString(
      @"IDS_IOS_UI_BLOCKED_USE_OTHER_WINDOW_DESCRIPTION", @"");
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
  [button
      setTitle:NSLocalizedString(
                   @"IDS_IOS_UI_BLOCKED_USE_OTHER_WINDOW_SWITCH_WINDOW_ACTION",
                   @"")
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
  [self.blockingSceneCommandHandler
      activateBlockingScene:self.view.window.windowScene];
}

@end
