// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/notification_dialog_view_controller.h"

#include <algorithm>

#import <MaterialComponents/MaterialButtons.h>
#import <MaterialComponents/MaterialDialogs.h>

#include "base/check.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#include "remoting/client/notification/notification_message.h"
#import "remoting/ios/app/remoting_theme.h"
#include "remoting/ios/app/view_utils.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kLabelInset = 20.f;
static const CGFloat kSwitchInset = 10.f;
static const CGFloat kButtonHeight = 48.f;
static const CGFloat kDontShowAgainFontSize = 14.f;
static const CGFloat kDontShowAgainViewHeightAdjustment = 10.f;

@implementation NotificationDialogViewController {
  NSString* _messageText;
  NSString* _linkText;
  NSURL* _linkUrl;
  BOOL _allowSilence;
  NotificationDialogCompletionBlock _completion;
  MDCDialogTransitionController* _transitionController;
  UILabel* _messageLabel;
  MDCButton* _linkButton;
  MDCButton* _dismissButton;

  // These will be nil if |_allowSilence| is NO.
  UISwitch* _dontShowAgainSwitch;
  UILabel* _dontShowAgainLabel;
}

#pragma mark - UIViewController

- (instancetype)initWithNotificationMessage:
                    (const remoting::NotificationMessage&)message
                               allowSilence:(BOOL)allowSilence {
  self = [super init];
  if (self) {
    _messageText = base::SysUTF8ToNSString(message.message_text);
    _linkText = base::SysUTF8ToNSString(message.link_text);
    _linkUrl = [NSURL URLWithString:base::SysUTF8ToNSString(message.link_url)];
    _allowSilence = allowSilence;

    _transitionController = [[MDCDialogTransitionController alloc] init];
    self.modalPresentationStyle = UIModalPresentationCustom;
    self.transitioningDelegate = _transitionController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = RemotingTheme.dialogBackgroundColor;

  _messageLabel = [[UILabel alloc] init];
  _messageLabel.textColor = RemotingTheme.dialogTextColor;
  _messageLabel.lineBreakMode = NSLineBreakByWordWrapping;
  _messageLabel.numberOfLines = 0;
  _messageLabel.text = _messageText;
  _messageLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_messageLabel];

  UIView* dontShowAgainView = [[UIView alloc] init];
  dontShowAgainView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:dontShowAgainView];

  _linkButton = [[MDCFlatButton alloc] init];
  [_linkButton setTitle:_linkText forState:UIControlStateNormal];
  [_linkButton addTarget:self
                  action:@selector(didTapLinkButton:)
        forControlEvents:UIControlEventTouchUpInside];
  [self addButton:_linkButton isPrimary:YES];

  _dismissButton = [[MDCFlatButton alloc] init];
  [_dismissButton setTitle:l10n_util::GetNSString(IDS_DISMISS)
                  forState:UIControlStateNormal];
  [_dismissButton addTarget:self
                     action:@selector(didTapDismissButton:)
           forControlEvents:UIControlEventTouchUpInside];
  [self addButton:_dismissButton isPrimary:NO];

  [NSLayoutConstraint activateConstraints:@[
    [_messageLabel.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                            constant:kLabelInset],
    [_messageLabel.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                                constant:kLabelInset],
    [_messageLabel.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kLabelInset],

    [dontShowAgainView.topAnchor
        constraintEqualToAnchor:_messageLabel.bottomAnchor
                       constant:kLabelInset],
    [dontShowAgainView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [dontShowAgainView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],

    [_dismissButton.topAnchor
        constraintEqualToAnchor:dontShowAgainView.bottomAnchor],
    [_dismissButton.heightAnchor constraintEqualToConstant:kButtonHeight],
    [_dismissButton.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_dismissButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.leadingAnchor],

    [_linkButton.topAnchor constraintEqualToAnchor:_dismissButton.topAnchor],
    [_linkButton.leadingAnchor
        constraintEqualToAnchor:_dismissButton.trailingAnchor],
    [_linkButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_linkButton.bottomAnchor
        constraintEqualToAnchor:_dismissButton.bottomAnchor],
    [_linkButton.heightAnchor constraintEqualToConstant:kButtonHeight],
  ]];

  if (_allowSilence) {
    // This is to allow user to toggle switch by tapping the label. Tap events
    // won't be bubbled down to the switch.
    UITapGestureRecognizer* dontShowAgainTapGestureRecognizer =
        [[UITapGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(didTapDontShowAgain:)];
    dontShowAgainTapGestureRecognizer.numberOfTapsRequired = 1;
    [dontShowAgainView addGestureRecognizer:dontShowAgainTapGestureRecognizer];
    dontShowAgainView.userInteractionEnabled = YES;

    NSString* dontShowAgainText = l10n_util::GetNSString(IDS_DONT_SHOW_AGAIN);

    _dontShowAgainSwitch = [[UISwitch alloc] init];
    _dontShowAgainSwitch.translatesAutoresizingMaskIntoConstraints = NO;
    _dontShowAgainSwitch.transform = CGAffineTransformMakeScale(0.5, 0.5);
    _dontShowAgainSwitch.accessibilityLabel = dontShowAgainText;
    [dontShowAgainView addSubview:_dontShowAgainSwitch];

    _dontShowAgainLabel = [[UILabel alloc] init];
    _dontShowAgainLabel.textColor = RemotingTheme.dialogTextColor;
    _dontShowAgainLabel.font = [UIFont systemFontOfSize:kDontShowAgainFontSize];
    _dontShowAgainLabel.text = dontShowAgainText;
    _dontShowAgainLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [dontShowAgainView addSubview:_dontShowAgainLabel];

    [NSLayoutConstraint activateConstraints:@[
      [dontShowAgainView.heightAnchor
          constraintEqualToAnchor:_dontShowAgainLabel.heightAnchor
                         constant:kDontShowAgainViewHeightAdjustment],

      [_dontShowAgainSwitch.leadingAnchor
          constraintEqualToAnchor:dontShowAgainView.leadingAnchor
                         constant:kSwitchInset],
      [_dontShowAgainSwitch.centerYAnchor
          constraintEqualToAnchor:dontShowAgainView.centerYAnchor],

      [_dontShowAgainLabel.leadingAnchor
          constraintEqualToAnchor:_dontShowAgainSwitch.trailingAnchor],
      [_dontShowAgainLabel.trailingAnchor
          constraintEqualToAnchor:dontShowAgainView.trailingAnchor
                         constant:-kLabelInset],
      [_dontShowAgainLabel.centerYAnchor
          constraintEqualToAnchor:_dontShowAgainSwitch.centerYAnchor],
    ]];
  }

  [self.view setNeedsLayout];
}

- (void)viewDidLayoutSubviews {
  CGFloat contentWidth =
      MAX(_messageLabel.intrinsicContentSize.width + 2 * kLabelInset,
          _dismissButton.intrinsicContentSize.width +
              _linkButton.intrinsicContentSize.width);
  CGFloat contentHeight = _messageLabel.intrinsicContentSize.height +
                          2 * kLabelInset + kButtonHeight;
  if (_allowSilence) {
    contentWidth =
        MAX(contentWidth,
            kSwitchInset + _dontShowAgainSwitch.intrinsicContentSize.width +
                _dontShowAgainLabel.intrinsicContentSize.width + kLabelInset);
    contentHeight += _dontShowAgainLabel.intrinsicContentSize.height +
                     kDontShowAgainViewHeightAdjustment;
  }
  self.preferredContentSize = CGSizeMake(contentWidth, contentHeight);
}

- (void)presentOnTopVCWithCompletion:
    (NotificationDialogCompletionBlock)completion {
  DCHECK(completion);
  DCHECK(!_completion);
  _completion = completion;
  [remoting::TopPresentingVC() presentViewController:self
                                            animated:YES
                                          completion:nil];
}

- (void)viewDidDisappear:(BOOL)animated {
  DCHECK(_completion);
  _completion(_dontShowAgainSwitch && _dontShowAgainSwitch.on);
  // Release the block as long as the dialog is disappeared, since it could
  // potentially have retain loop.
  _completion = nil;
}

#pragma mark - Private

- (void)addButton:(UIButton*)button isPrimary:(BOOL)isPrimary {
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.backgroundColor = UIColor.clearColor;
  UIColor* titleColor = isPrimary
                            ? RemotingTheme.dialogPrimaryButtonTextColor
                            : RemotingTheme.dialogSecondaryButtonTextColor;
  [button setTitleColor:titleColor forState:UIControlStateNormal];
  [self.view addSubview:button];
}

- (void)didTapLinkButton:(id)button {
  NSURL* linkUrl = _linkUrl;
  [self dismissViewControllerAnimated:YES
                           completion:^() {
                             [UIApplication.sharedApplication openURL:linkUrl
                                                              options:@{}
                                                    completionHandler:nil];
                           }];
}

- (void)didTapDismissButton:(id)button {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)didTapDontShowAgain:(id)sender {
  [_dontShowAgainSwitch setOn:!_dontShowAgainSwitch.isOn animated:YES];
}

@end
