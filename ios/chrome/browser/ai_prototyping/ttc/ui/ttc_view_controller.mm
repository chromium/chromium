// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Layout constants.
constexpr CGFloat kHeaderStackSpacing = 8.0;

// UI string constants.
NSString* const kHeaderTitleText = @"TalkToChrome";

}  // namespace

@implementation TTCViewController

@synthesize feature = _feature;
@synthesize mutator = _mutator;

#pragma mark - Initialization

- (instancetype)initForFeature:(AIPrototypingFeature)feature {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _feature = feature;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent],
  ];

  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  // Header Title.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.text = kHeaderTitleText;

  UIStackView* headerStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ titleLabel ]];
  headerStack.translatesAutoresizingMaskIntoConstraints = NO;
  headerStack.axis = UILayoutConstraintAxisVertical;
  headerStack.spacing = kHeaderStackSpacing;

  [self.view addSubview:headerStack];

  [NSLayoutConstraint activateConstraints:@[
    [headerStack.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                          constant:kMainStackTopInset],
    [headerStack.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                              constant:kHorizontalInset],
    [headerStack.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                               constant:-kHorizontalInset],
  ]];
}

@end
