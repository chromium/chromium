// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/table_view_loading_view.h"

#import <MaterialComponents/MaterialActivityIndicator.h>

#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// The MDCActivityIndicator radius.
const float kLoadingIndicatorRadius = 9.0;
// The StackView width.
const float kStackViewWidth = 227.0;
// The StackView vertical spacing.
const float kStackViewVerticalSpacing = 30.0;
}  // namespace

@interface TableViewLoadingView () <MDCActivityIndicatorDelegate>
// MDCActivityIndicator that will be displayed.
@property(nonatomic, strong) MDCActivityIndicator* activityIndicator;
// Completion block ran after `self.activityIndicator` stops.
@property(nonatomic, copy) ProceduralBlock animateOutCompletionBlock;
// Message being displayed along the activity indicator.
@property(nonatomic, copy) NSString* loadingMessage;
@end

@implementation TableViewLoadingView
@synthesize activityIndicator = _activityIndicator;
@synthesize animateOutCompletionBlock = _animateOutCompletionBlock;
@synthesize loadingMessage = _loadingMessage;

#pragma mark - Public Interface

- (instancetype)initWithFrame:(CGRect)frame {
  return [self initWithFrame:frame loadingMessage:nil];
}

- (instancetype)initWithFrame:(CGRect)frame loadingMessage:(NSString*)message {
  self = [super initWithFrame:frame];
  if (self) {
    self.loadingMessage = message;
  }
  return self;
}

- (void)startLoadingIndicator {
  self.activityIndicator =
      [[MDCActivityIndicator alloc] initWithFrame:CGRectZero];
  self.activityIndicator.radius = kLoadingIndicatorRadius;
  self.activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  self.activityIndicator.cycleColors = @[ [UIColor colorNamed:kBlueColor] ];
  self.activityIndicator.delegate = self;

  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.text = self.loadingMessage;
  messageLabel.numberOfLines = 0;
  messageLabel.lineBreakMode = NSLineBreakByWordWrapping;
  messageLabel.textAlignment = NSTextAlignmentCenter;
  messageLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  messageLabel.textColor = [UIColor grayColor];

  // Vertical stack view that holds the activity indicator and message.
  UIStackView* verticalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.activityIndicator, messageLabel ]];
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
  [self.activityIndicator startAnimating];
}

- (void)stopLoadingIndicatorWithCompletion:(ProceduralBlock)completion {
  if (self.activityIndicator) {
    self.animateOutCompletionBlock = completion;
    [self.activityIndicator stopAnimating];
  } else if (completion) {
    completion();
  }
}

#pragma mark - MDCActivityIndicatorDelegate

- (void)activityIndicatorAnimationDidFinish:
    (MDCActivityIndicator*)activityIndicator {
  [self.activityIndicator removeFromSuperview];
  self.activityIndicator = nil;
  if (self.animateOutCompletionBlock) {
    self.animateOutCompletionBlock();
  }
  self.animateOutCompletionBlock = nil;
}

@end
