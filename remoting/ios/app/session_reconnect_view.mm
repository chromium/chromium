// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/session_reconnect_view.h"

#import <MaterialComponents/MaterialButtons.h>

#include "remoting/base/string_resources.h"
#import "remoting/ios/app/remoting_theme.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kPadding = 20.f;
static UIFont* const kErrorFont = [UIFont systemFontOfSize:13.f];

@interface SessionReconnectView () {
  MDCFloatingButton* _reconnectButton;
  UILabel* _errorLabel;
  UILabel* _reportThisLabel;
}
@end

@implementation SessionReconnectView

@synthesize delegate = _delegate;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];

    _reconnectButton =
        [MDCFloatingButton floatingButtonWithShape:MDCFloatingButtonShapeMini];
    [_reconnectButton
        setImage:[RemotingTheme
                         .refreshIcon imageFlippedForRightToLeftLayoutDirection]
        forState:UIControlStateNormal];
    [_reconnectButton setBackgroundColor:RemotingTheme.buttonBackgroundColor
                                forState:UIControlStateNormal];

    [_reconnectButton setElevation:4.0f forState:UIControlStateNormal];
    [_reconnectButton setTitle:l10n_util::GetNSString(IDS_RECONNECT)
                      forState:UIControlStateNormal];
    [_reconnectButton addTarget:self
                         action:@selector(didTapReconnect:)
               forControlEvents:UIControlEventTouchUpInside];
    _reconnectButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_reconnectButton];

    _errorLabel = [[UILabel alloc] init];
    _errorLabel.textColor = RemotingTheme.connectionViewForegroundColor;
    _errorLabel.font = kErrorFont;
    _errorLabel.numberOfLines = 0;
    _errorLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _errorLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_errorLabel];

    _reportThisLabel = [[UILabel alloc] init];
    _reportThisLabel.accessibilityTraits = UIAccessibilityTraitLink;
    _reportThisLabel.text = l10n_util::GetNSString(IDS_REPORT_THIS);
    _reportThisLabel.textColor = RemotingTheme.hostErrorColor;
    _reportThisLabel.font = kErrorFont;
    _reportThisLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_reportThisLabel];

    UITapGestureRecognizer* tapReportRecognizer =
        [[UITapGestureRecognizer alloc] init];
    tapReportRecognizer.numberOfTapsRequired = 1;
    tapReportRecognizer.numberOfTouchesRequired = 1;
    [tapReportRecognizer addTarget:self action:@selector(didTapReport:)];
    _reportThisLabel.userInteractionEnabled = YES;
    [_reportThisLabel addGestureRecognizer:tapReportRecognizer];

    [self setupLayoutConstraints];
  }
  return self;
}

- (void)setupLayoutConstraints {
  UILayoutGuide* errorTextLayoutGuide = [[UILayoutGuide alloc] init];
  [self addLayoutGuide:errorTextLayoutGuide];

  [NSLayoutConstraint activateConstraints:@[
    [errorTextLayoutGuide.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
    [errorTextLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],

    [_errorLabel.topAnchor
        constraintEqualToAnchor:errorTextLayoutGuide.topAnchor],
    [_errorLabel.leadingAnchor
        constraintEqualToAnchor:errorTextLayoutGuide.leadingAnchor],
    [_errorLabel.trailingAnchor
        constraintEqualToAnchor:errorTextLayoutGuide.trailingAnchor],

    [_reportThisLabel.topAnchor
        constraintEqualToAnchor:_errorLabel.bottomAnchor],
    [_reportThisLabel.leadingAnchor
        constraintEqualToAnchor:errorTextLayoutGuide.leadingAnchor],
    [_reportThisLabel.bottomAnchor
        constraintEqualToAnchor:errorTextLayoutGuide.bottomAnchor],
    // _reportThisLabel's width should freely expand for its content.

    [_reconnectButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_reconnectButton.leadingAnchor
        constraintEqualToAnchor:errorTextLayoutGuide.trailingAnchor
                       constant:kPadding],
    [_reconnectButton.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
  ]];
}

#pragma mark - Properties

- (void)setErrorText:(NSString*)errorText {
  if (errorText) {
    _errorLabel.text = errorText;
    _errorLabel.hidden = NO;
  } else {
    _errorLabel.hidden = YES;
  }
}

- (NSString*)errorText {
  return _errorLabel.text;
}

#pragma mark - Private

- (void)didTapReconnect:(id)sender {
  if ([_delegate respondsToSelector:@selector(didTapReconnect)]) {
    [_delegate didTapReconnect];
  }
}

- (void)didTapReport:(UITapGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateEnded) {
    [self.delegate didTapReport];
  }
}

@end
