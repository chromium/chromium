// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_controller.h"

#import <ostream>

#import "base/ios/ios_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/instruction_view.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_action_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_detail_view_delegate.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/sdk_forward_declares.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

constexpr CGFloat kDefaultMargin = 32;
constexpr CGFloat kSubtitleTopMargin = 8;
constexpr CGFloat kActionsBottomMargin = 10;
constexpr CGFloat kContentWidthMultiplier = 0.8;
constexpr CGFloat kButtonHorizontalMargin = 4;
constexpr CGFloat kSeparatorHeight = 1;
constexpr CGFloat kLabelMinimumScaleFactor = 0.7;

// Accessibility Identifier.
NSString* const kWhatsNewTitleAccessibilityIdentifier =
    @"WhatsNewTitleAccessibilityIdentifier";
NSString* const kWhatsNewSubtitleAccessibilityIdentifier =
    @"WhatsNewSubtitleAccessibilityIdentifier";
NSString* const kWhatsNewPrimaryActionAccessibilityIdentifier =
    @"WhatsNewPrimaryActionAccessibilityIdentifier";
NSString* const kWhatsNewLearnMoreActionAccessibilityIdentifier =
    @"WhatsNewDisclaimerViewAccessibilityIdentifier";
NSString* const kWhatsNewScrollViewAccessibilityIdentifier =
    @"WhatsNewScrollViewAccessibilityIdentifier";

}  // namespace

@interface WhatsNewDetailViewController () <UIScrollViewDelegate>

// Visible UI components.
@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) UIImageView* imageView;
@property(nonatomic, strong) UILabel* subtitleLabel;
@property(nonatomic, strong) HighlightButton* primaryActionButton;
@property(nonatomic, strong) UIButton* learnMoreActionButton;
@property(nonatomic, strong) UILabel* titleLabel;

// Properties set on initialization.
@property(nonatomic, copy) UIImage* bannerImage;
@property(nonatomic, copy) NSString* titleText;
@property(nonatomic, copy) NSString* subtitleText;
@property(nonatomic, copy) NSString* primaryActionString;
@property(nonatomic, copy) NSArray<NSString*>* instructionSteps;
@property(nonatomic, assign) WhatsNewType type;
@property(nonatomic, assign) WhatsNewPrimaryAction primaryAction;
@property(nonatomic, assign) GURL learnMoreURL;

// The navigation bar at the top of the view.
@property(nonatomic, strong) UINavigationBar* navigationBar;

@end

@implementation WhatsNewDetailViewController

- (instancetype)initWithParams:(UIImage*)image
                         title:(NSString*)title
                      subtitle:(NSString*)subtitle
            primaryActionTitle:(NSString*)primaryActionTitle
              instructionSteps:(NSArray<NSString*>*)instructionSteps
                          type:(WhatsNewType)type
                 primaryAction:(WhatsNewPrimaryAction)primaryAction
                  learnMoreURL:(const GURL&)learnMoreURL {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _bannerImage = image;
    _titleText = title;
    _subtitleText = subtitle;
    _primaryActionString = primaryActionTitle;
    _instructionSteps = instructionSteps;
    _type = type;
    _primaryAction = primaryAction;
    _learnMoreURL = learnMoreURL;
  }
  return self;
}

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];

  self.scrollView.delegate = self;
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  UIView* instructionView =
      [[InstructionView alloc] initWithList:self.instructionSteps];
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* separator = [[UIView alloc] init];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  separator.hidden = YES;
  [self.view addSubview:separator];

  UIView* scrollContentView = [[UIView alloc] init];
  scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollContentView addSubview:self.imageView];
  [scrollContentView addSubview:self.titleLabel];
  [scrollContentView addSubview:self.subtitleLabel];
  [scrollContentView addSubview:instructionView];
  [self.scrollView addSubview:scrollContentView];
  [self.view addSubview:self.scrollView];

  UIStackView* actionStackView = [[UIStackView alloc] init];
  actionStackView.alignment = UIStackViewAlignmentFill;
  actionStackView.axis = UILayoutConstraintAxisVertical;
  actionStackView.translatesAutoresizingMaskIntoConstraints = NO;
  if (self.primaryActionString) {
    [actionStackView addArrangedSubview:self.primaryActionButton];
  }
  if (self.learnMoreURL.is_valid()) {
    [actionStackView addArrangedSubview:self.learnMoreActionButton];
  }

  [self.view addSubview:actionStackView];

  // Create a layout guide to constrain the width of the content.
  UILayoutGuide* widthLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:widthLayoutGuide];

  [scrollContentView.bottomAnchor
      constraintEqualToAnchor:instructionView.bottomAnchor]
      .active = YES;

  [NSLayoutConstraint activateConstraints:@[
    // Content width layout guide constraints.
    [widthLayoutGuide.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [widthLayoutGuide.widthAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.widthAnchor
                                  multiplier:kContentWidthMultiplier],
    [widthLayoutGuide.widthAnchor
        constraintLessThanOrEqualToAnchor:self.view.widthAnchor
                                 constant:-2 * kDefaultMargin],

    // Scroll view constraints.
    [self.scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.scrollView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.scrollView.bottomAnchor
        constraintEqualToAnchor:actionStackView.topAnchor
                       constant:-kDefaultMargin],

    // Separator constraints.
    [separator.heightAnchor constraintEqualToConstant:kSeparatorHeight],
    [separator.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [separator.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [separator.topAnchor constraintEqualToAnchor:self.scrollView.bottomAnchor],

    // Scroll content view constraints.
    [scrollContentView.topAnchor
        constraintEqualToAnchor:self.scrollView.topAnchor],
    [scrollContentView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [scrollContentView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],
    [scrollContentView.bottomAnchor
        constraintEqualToAnchor:self.scrollView.bottomAnchor],

    // Image view constraints.
    [self.imageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.imageView.topAnchor
        constraintEqualToAnchor:scrollContentView.topAnchor],

    [actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                 constant:-kActionsBottomMargin * 2],

    // Title contraints.
    [self.titleLabel.topAnchor
        constraintEqualToAnchor:self.imageView.bottomAnchor],
    [self.titleLabel.centerXAnchor
        constraintEqualToAnchor:scrollContentView.centerXAnchor],
    [self.titleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:scrollContentView.widthAnchor],

    // Subtitle contraints.
    [self.subtitleLabel.topAnchor
        constraintEqualToAnchor:self.titleLabel.bottomAnchor
                       constant:kSubtitleTopMargin],
    [self.subtitleLabel.centerXAnchor
        constraintEqualToAnchor:scrollContentView.centerXAnchor],
    [self.subtitleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:scrollContentView.widthAnchor],

    // Instructions contraints.
    [instructionView.topAnchor
        constraintEqualToAnchor:self.subtitleLabel.bottomAnchor
                       constant:kDefaultMargin],
    [instructionView.centerXAnchor
        constraintEqualToAnchor:scrollContentView.centerXAnchor],
    [instructionView.leadingAnchor
        constraintEqualToAnchor:scrollContentView.leadingAnchor],
    [instructionView.trailingAnchor
        constraintEqualToAnchor:scrollContentView.trailingAnchor],

    // Action stack view constraints.
    [actionStackView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [actionStackView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],

  ]];

  // Constrain the bottom of the action stack view to the bottom of the
  // view with a lower priority.
  NSLayoutConstraint* actionBottomConstraint = [actionStackView.bottomAnchor
      constraintEqualToAnchor:self.view.bottomAnchor];
  actionBottomConstraint.priority = UILayoutPriorityDefaultLow;
  actionBottomConstraint.active = YES;
}

- (void)viewDidDisappear:(BOOL)animated {
  self.navigationBar = nil;
  self.actionHandler = nil;
  [super viewDidDisappear:animated];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  if ([self.navigationController
          isKindOfClass:[UINavigationController class]]) {
    UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                      primaryAction:[UIAction actionWithHandler:^(
                                                  UIAction* action) {
                        [self.parentViewController
                            dismissViewControllerAnimated:YES
                                               completion:nil];
                      }]];
    self.navigationItem.rightBarButtonItem = doneButton;

    self.navigationBar = self.navigationController.navigationBar;
  }
}

#pragma mark - Accessors

- (UIScrollView*)scrollView {
  if (!_scrollView) {
    _scrollView = [[UIScrollView alloc] init];
    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrollView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
    _scrollView.accessibilityIdentifier =
        kWhatsNewScrollViewAccessibilityIdentifier;
  }

  return _scrollView;
}

- (UIImageView*)imageView {
  if (!_imageView) {
    _imageView = [[UIImageView alloc] initWithImage:self.bannerImage];
    _imageView.clipsToBounds = YES;
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  }

  return _imageView;
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 0;
    _titleLabel.font =
        CreateDynamicFont(UIFontTextStyleTitle1, UIFontWeightBold);
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.text = self.titleText;
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.accessibilityIdentifier = kWhatsNewTitleAccessibilityIdentifier;
    _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  }

  return _titleLabel;
}

- (UILabel*)subtitleLabel {
  if (!_subtitleLabel) {
    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _subtitleLabel.numberOfLines = 0;
    _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitleLabel.text = self.subtitleText;
    _subtitleLabel.textAlignment = NSTextAlignmentCenter;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.accessibilityIdentifier =
        kWhatsNewSubtitleAccessibilityIdentifier;
  }

  return _subtitleLabel;
}

- (UIButton*)primaryActionButton {
  if (!_primaryActionButton) {
    if (IsUIButtonConfigurationEnabled()) {
      UIButtonConfiguration* buttonConfiguration =
          [UIButtonConfiguration plainButtonConfiguration];

      // TODO(crbug.com/1466965): Replace kButtonHorizontalMargin.
      CGFloat newButtonHorizontalMargin = kButtonHorizontalMargin + 10;
      buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
          newButtonHorizontalMargin, 0, newButtonHorizontalMargin, 0);
      UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      NSDictionary* attributes = @{NSFontAttributeName : font};
      NSMutableAttributedString* attributedString =
          [[NSMutableAttributedString alloc]
              initWithString:self.primaryActionString
                  attributes:attributes];
      buttonConfiguration.attributedTitle = attributedString;
      buttonConfiguration.baseForegroundColor =
          [UIColor colorNamed:kSolidButtonTextColor];
      buttonConfiguration.background.backgroundColor =
          [UIColor colorNamed:kBlueColor];
      buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;
      buttonConfiguration.background.cornerRadius = kPrimaryButtonCornerRadius;
      _primaryActionButton =
          [HighlightButton buttonWithConfiguration:buttonConfiguration
                                     primaryAction:nil];
    } else {
      _primaryActionButton = [[HighlightButton alloc] initWithFrame:CGRectZero];
      [_primaryActionButton setTitle:self.primaryActionString
                            forState:UIControlStateNormal];
      [_primaryActionButton setBackgroundColor:[UIColor colorNamed:kBlueColor]];
      [_primaryActionButton
          setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
               forState:UIControlStateNormal];
      _primaryActionButton.titleLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      _primaryActionButton.titleLabel.adjustsFontForContentSizeCategory = YES;
      _primaryActionButton.titleLabel.adjustsFontSizeToFitWidth = YES;
      _primaryActionButton.titleLabel.minimumScaleFactor =
          kLabelMinimumScaleFactor;
      _primaryActionButton.titleLabel.lineBreakMode =
          NSLineBreakByTruncatingTail;
      UIEdgeInsets titleInsets = UIEdgeInsetsMake(0, kButtonHorizontalMargin, 0,
                                                  kButtonHorizontalMargin);
      SetTitleEdgeInsets(_primaryActionButton, titleInsets);
      UIEdgeInsets contentInsets =
          UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
      SetContentEdgeInsets(_primaryActionButton, contentInsets);
      _primaryActionButton.layer.cornerRadius = kPrimaryButtonCornerRadius;
    }

    _primaryActionButton.accessibilityIdentifier =
        kWhatsNewPrimaryActionAccessibilityIdentifier;
    _primaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;
    _primaryActionButton.pointerInteractionEnabled = YES;
    _primaryActionButton.pointerStyleProvider =
        CreateOpaqueButtonPointerStyleProvider();
    [_primaryActionButton addTarget:self
                             action:@selector(didTapPrimaryActionButton)
                   forControlEvents:UIControlEventTouchUpInside];
  }

  return _primaryActionButton;
}

- (UIButton*)learnMoreActionButton {
  if (!_learnMoreActionButton) {
    NSString* learnMoreText =
        l10n_util::GetNSString(IDS_IOS_WHATS_NEW_LEARN_MORE_ACTION_TITLE);
    _learnMoreActionButton = [UIButton buttonWithType:UIButtonTypeSystem];

    if (IsUIButtonConfigurationEnabled()) {
      UIButtonConfiguration* buttonConfiguration =
          [UIButtonConfiguration plainButtonConfiguration];
      _learnMoreActionButton.configuration.contentInsets =
          NSDirectionalEdgeInsetsMake(0, kButtonHorizontalMargin, 0,
                                      kButtonHorizontalMargin);
      UIFont* font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      NSDictionary* attributes = @{NSFontAttributeName : font};
      NSMutableAttributedString* attributedString =
          [[NSMutableAttributedString alloc] initWithString:learnMoreText
                                                 attributes:attributes];
      buttonConfiguration.attributedTitle = attributedString;
      buttonConfiguration.background.backgroundColor = [UIColor clearColor];
      buttonConfiguration.baseForegroundColor = [UIColor colorNamed:kBlueColor];
      buttonConfiguration.titleLineBreakMode = NSLineBreakByTruncatingTail;

      _learnMoreActionButton.configuration = buttonConfiguration;
    } else {
      [_learnMoreActionButton setTitle:learnMoreText
                              forState:UIControlStateNormal];
      [_learnMoreActionButton setBackgroundColor:[UIColor clearColor]];
      [_learnMoreActionButton setTitleColor:[UIColor colorNamed:kBlueColor]
                                   forState:UIControlStateNormal];
      _learnMoreActionButton.titleLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
      _learnMoreActionButton.titleLabel.adjustsFontForContentSizeCategory = YES;
      _learnMoreActionButton.titleLabel.adjustsFontSizeToFitWidth = YES;
      _learnMoreActionButton.titleLabel.minimumScaleFactor =
          kLabelMinimumScaleFactor;
      _learnMoreActionButton.titleLabel.lineBreakMode =
          NSLineBreakByTruncatingTail;
      UIEdgeInsets titleInsets = UIEdgeInsetsMake(0, kButtonHorizontalMargin, 0,
                                                  kButtonHorizontalMargin);
      SetTitleEdgeInsets(_learnMoreActionButton, titleInsets);
    }

    _learnMoreActionButton.accessibilityIdentifier =
        kWhatsNewLearnMoreActionAccessibilityIdentifier;
    _learnMoreActionButton.translatesAutoresizingMaskIntoConstraints = NO;
    _learnMoreActionButton.pointerInteractionEnabled = YES;
    _learnMoreActionButton.pointerStyleProvider =
        CreateOpaqueButtonPointerStyleProvider();
    [_learnMoreActionButton addTarget:self
                               action:@selector(didTaplearnMoreActionButton)
                     forControlEvents:UIControlEventTouchUpInside];
  }

  return _learnMoreActionButton;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  DCHECK_EQ(self.scrollView, scrollView);
  if ([self shouldRemoveNavBarTranslucent]) {
    self.navigationBar.translucent = NO;
  } else {
    self.navigationBar.translucent = YES;
  }
}

#pragma mark - Private

- (void)didTapPrimaryActionButton {
  [self.actionHandler didTapActionButton:self.type
                           primaryAction:self.primaryAction];
}

- (void)didTaplearnMoreActionButton {
  [self.actionHandler didTapLearnMoreButton:self.learnMoreURL type:self.type];
  [self.delegate dismissWhatsNewDetailView:self];
}

- (BOOL)shouldRemoveNavBarTranslucent {
  // The navbar should not be translucent if the user scrolls pass the banner
  // image.
  return self.scrollView.contentOffset.y >=
         self.imageView.bounds.size.height -
             self.navigationBar.bounds.size.height;
}

@end
