// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/table_view_loading_view.h"

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Scale of activity indicator replacing fav icon when active.
const CGFloat kIndicatorScale = 0.75;
// The StackView width.
const float kStackViewWidth = 227.0;
// The StackView vertical spacing.
const float kStackViewVerticalSpacing = 30.0;

}  // namespace

@implementation TableViewLoadingView {
  // Activity indicator that will be displayed.
  UIActivityIndicatorView* _activityIndicator;
  // Message being displayed along the activity indicator.
  NSString* _loadingMessage;
}

#pragma mark - Public Interface

- (instancetype)initWithFrame:(CGRect)frame {
  return [self initWithFrame:frame loadingMessage:nil];
}

- (instancetype)initWithFrame:(CGRect)frame loadingMessage:(NSString*)message {
  self = [super initWithFrame:frame];
  if (self) {
    _loadingMessage = message;
  }
  return self;
}

- (void)startLoadingIndicator {
  _activityIndicator = [[UIActivityIndicatorView alloc] init];
  _activityIndicator.color = [UIColor colorNamed:kBlueColor];
  _activityIndicator.transform = CGAffineTransformScale(
      _activityIndicator.transform, kIndicatorScale, kIndicatorScale);
  _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_activityIndicator];
  [_activityIndicator startAnimating];

  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.text = _loadingMessage;
  messageLabel.numberOfLines = 0;
  messageLabel.lineBreakMode = NSLineBreakByWordWrapping;
  messageLabel.textAlignment = NSTextAlignmentCenter;
  messageLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  messageLabel.textColor = [UIColor grayColor];

  // Vertical stack view that holds the activity indicator and message.
  UIStackView* verticalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _activityIndicator, messageLabel ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = kStackViewVerticalSpacing;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:verticalStack];

  [NSLayoutConstraint activateConstraints:@[
    [verticalStack.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [verticalStack.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [verticalStack.widthAnchor constraintEqualToConstant:kStackViewWidth]
  ]];
  [_activityIndicator startAnimating];
}

- (void)stopLoadingIndicatorWithCompletion:(ProceduralBlock)completion {
  [_activityIndicator stopAnimating];
  if (completion) {
    completion();
  }
}

@end
