// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/host_setup_footer_view.h"

#import <MaterialComponents/MaterialButtons.h>
#import <MaterialComponents/MaterialTypography.h>

#import "remoting/ios/app/app_delegate.h"
#import "remoting/ios/app/remoting_theme.h"
#include "remoting/base/string_resources.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kTopPadding = 6.f;

@implementation HostSetupFooterView

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    [self commonInit];
  }
  return self;
}

- (void)commonInit {
  self.backgroundColor = RemotingTheme.setupListBackgroundColor;

  MDCRaisedButton* raisedButton = [[MDCRaisedButton alloc] init];

  [raisedButton
      setTitle:l10n_util::GetNSString(IDS_EMAIL_LINKS_AND_INSTRUCTIONS)
      forState:UIControlStateNormal];
  [raisedButton setTitleColor:RemotingTheme.buttonTextColor
                     forState:UIControlStateNormal];
  [raisedButton setBackgroundColor:RemotingTheme.buttonBackgroundColor
                          forState:UIControlStateNormal];
  [raisedButton sizeToFit];
  [raisedButton addTarget:self
                   action:@selector(didTapEmailInstructions:)
         forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:raisedButton];
  raisedButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [raisedButton.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [raisedButton.topAnchor constraintEqualToAnchor:self.topAnchor
                                           constant:kTopPadding],
  ]];
}

- (void)didTapEmailInstructions:(id)button {
  [AppDelegate.instance emailSetupInstructions];
}

@end
