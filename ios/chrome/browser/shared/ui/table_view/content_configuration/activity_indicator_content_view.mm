// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/activity_indicator_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/activity_indicator_content_configuration.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation ActivityIndicatorCellContentView {
  ActivityIndicatorContentConfiguration* _configuration;
  UIActivityIndicatorView* _activityIndicator;

  NSLayoutConstraint* _heightConstraint;
  NSLayoutConstraint* _widthConstraint;
}

- (instancetype)initWithConfiguration:
    (ActivityIndicatorContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _configuration = [configuration copy];

    // The correct value will be set when the config is applied.
    _widthConstraint = [self.widthAnchor constraintEqualToConstant:0];
    _heightConstraint = [self.heightAnchor constraintEqualToConstant:0];

    [self setupViews];
    [self applyConfiguration];

    self.translatesAutoresizingMaskIntoConstraints = NO;

    [NSLayoutConstraint
        activateConstraints:@[ _widthConstraint, _heightConstraint ]];
  }
  return self;
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return NO;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  ActivityIndicatorContentConfiguration* chromeConfiguration =
      base::apple::ObjCCastStrict<ActivityIndicatorContentConfiguration>(
          configuration);
  _configuration = [chromeConfiguration copy];
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration
      isMemberOfClass:ActivityIndicatorContentConfiguration.class];
}

#pragma mark - Private

// Updates the content view with the current configuration.
- (void)applyConfiguration {
  _heightConstraint.constant = [_configuration contentSize].height;
  _widthConstraint.constant = [_configuration contentSize].width;
  _activityIndicator.activityIndicatorViewStyle = _configuration.style;
  if (_configuration.color) {
    _activityIndicator.color = _configuration.color;
  }
  if (_configuration.animating) {
    [_activityIndicator startAnimating];
  } else {
    [_activityIndicator stopAnimating];
  }
}

// Adds and configures the subviews.
- (void)setupViews {
  _activityIndicator = [[UIActivityIndicatorView alloc] init];
  _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_activityIndicator];

  AddSameCenterConstraints(_activityIndicator, self);
}

@end
