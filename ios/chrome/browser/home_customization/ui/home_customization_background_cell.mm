// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/logo_vendor.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

// Define constants within the namespace
namespace {

// Corner radius applied to the content view.
const CGFloat kContentViewCornerRadius = 10.88;

// Corner radius applied to the highlight border view.
const CGFloat kHighlightCornerRadius = 16.0;

// Top margin between contentView and borderWrapperView.
const CGFloat kContentViewTopMargin = 12.0;

// Border width for the highlight state.
const CGFloat kHighlightBorderWidth = 7.0;

// Border width for the gap between content and borders.
const CGFloat kGapBorderWidth = 3.78;

// Relative vertical position multiplier for the logoView within its container.
const CGFloat kLogoTopMultiplier = 0.24;

// Fixed width and height for the logoView.
const CGFloat kLogoWidth = 34.0;

// Fixed height for the logoView.
const CGFloat kLogoHeight = 11.0;

}  // namespace

@interface HomeCustomizationBackgroundCell ()

// Container view that provides the outer highlight border.
// Acts as a decorative wrapper for the inner content.
@property(nonatomic, strong) UIView* borderWrapperView;

// Main content view rendered inside the border wrapper.
// Displays the core visual element.
@property(nonatomic, strong) UIStackView* innerContentView;

@end

@implementation HomeCustomizationBackgroundCell {
  // Associated background configuration.
  BackgroundCustomizationConfiguration* _backgroundConfiguration;

  // The background image of the cell.
  UIImageView* _backgroundImageView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.backgroundColor = UIColor.clearColor;

    // Outer container view that holds the highlight border.
    self.borderWrapperView = [[UIView alloc] init];
    self.borderWrapperView.translatesAutoresizingMaskIntoConstraints = NO;
    self.borderWrapperView.backgroundColor = UIColor.clearColor;
    self.borderWrapperView.layer.cornerRadius = kHighlightCornerRadius;
    self.borderWrapperView.layer.masksToBounds = YES;
    [self.contentView addSubview:self.borderWrapperView];

    // Inner content view, placed with a gap inside the border wrapper view.
    // This holds the actual content.
    self.innerContentView = [[UIStackView alloc] init];
    self.innerContentView.translatesAutoresizingMaskIntoConstraints = NO;
    self.innerContentView.backgroundColor =
        [UIColor colorNamed:@"ntp_background_color"];
    self.innerContentView.layer.cornerRadius = kContentViewCornerRadius;
    self.innerContentView.layer.masksToBounds = YES;
    self.innerContentView.axis = UILayoutConstraintAxisVertical;

    // Adds the empty background image.
    _backgroundImageView = [[UIImageView alloc] init];
    _backgroundImageView.contentMode = UIViewContentModeScaleAspectFill;
    _backgroundImageView.clipsToBounds = YES;
    _backgroundImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.innerContentView addSubview:_backgroundImageView];
    AddSameConstraints(_backgroundImageView, self.innerContentView);

    [self.borderWrapperView addSubview:self.innerContentView];

    // Constraints for positioning the border wrapper view inside the cell.
    [NSLayoutConstraint activateConstraints:@[
      [self.borderWrapperView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kContentViewTopMargin],
      [self.borderWrapperView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [self.borderWrapperView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      [self.borderWrapperView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
    ]];

    AddSameConstraintsWithInset(self.innerContentView, self.borderWrapperView,
                                kGapBorderWidth + kHighlightBorderWidth);
  }
  return self;
}

- (void)configureWithBackgroundOption:
            (BackgroundCustomizationConfiguration*)option
                           logoVendor:(id<LogoVendor>)logoVendor {
  _backgroundConfiguration = option;
  UIView* logoView = logoVendor.view;

  [self.innerContentView addSubview:logoView];
  logoVendor.view.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [NSLayoutConstraint constraintWithItem:logoView
                                 attribute:NSLayoutAttributeTop
                                 relatedBy:NSLayoutRelationEqual
                                    toItem:self.innerContentView
                                 attribute:NSLayoutAttributeBottom
                                multiplier:kLogoTopMultiplier
                                  constant:0],
    [logoView.centerXAnchor
        constraintEqualToAnchor:self.innerContentView.centerXAnchor],
    [logoView.widthAnchor constraintEqualToConstant:kLogoWidth],
    [logoView.heightAnchor constraintEqualToConstant:kLogoHeight]
  ]];
}

- (void)updateBackgroundImage:(UIImage*)image {
  [_backgroundImageView setImage:image];
}

#pragma mark - Setters

- (void)setSelected:(BOOL)selected {
  if (self.selected == selected) {
    return;
  }

  [super setSelected:selected];

  if (selected) {
    self.borderWrapperView.layer.borderColor =
        [UIColor colorNamed:kStaticBlueColor].CGColor;
    self.borderWrapperView.layer.borderWidth = kHighlightBorderWidth;
  } else {
    self.borderWrapperView.layer.borderColor = nil;
    self.borderWrapperView.layer.borderWidth = 0;
  }
}

@end
