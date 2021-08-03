// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller.h"

#include "base/check.h"
#include "base/i18n/rtl.h"
#import "ios/chrome/browser/ui/first_run/highlighted_button.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kFirstRunTitleAccessibilityIdentifier =
    @"kFirstRunTitleAccessibilityIdentifier";
NSString* const kFirstRunSubtitleAccessibilityIdentifier =
    @"kFirstRunSubtitleAccessibilityIdentifier";
NSString* const kFirstRunPrimaryActionAccessibilityIdentifier =
    @"kFirstRunPrimaryActionAccessibilityIdentifier";
NSString* const kFirstRunSecondaryActionAccessibilityIdentifier =
    @"kFirstRunSecondaryActionAccessibilityIdentifier";
NSString* const kFirstRunTertiaryActionAccessibilityIdentifier =
    @"kFirstRunTertiaryActionAccessibilityIdentifier";
NSString* const kFirstRunScrollViewAccessibilityIdentifier =
    @"kFirstRunScrollViewAccessibilityIdentifier";

namespace {

constexpr CGFloat kDefaultMargin = 16;
constexpr CGFloat kActionsBottomMargin = 10;
constexpr CGFloat kTallBannerMultiplier = 0.35;
constexpr CGFloat kDefaultBannerMultiplier = 0.25;
constexpr CGFloat kContentWidthMultiplier = 0.65;
constexpr CGFloat kContentMaxWidth = 327;
constexpr CGFloat kMoreArrowMargin = 4;
constexpr CGFloat kPreviousContentVisibleOnScroll = 0.15;

}  // namespace

@interface FirstRunScreenViewController () <UIScrollViewDelegate>

@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) UIImageView* imageView;
// UIView that wraps the scrollable content.
@property(nonatomic, strong) UIView* scrollContentView;
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UILabel* subtitleLabel;
@property(nonatomic, strong) HighlightedButton* primaryActionButton;
@property(nonatomic, strong) UIButton* secondaryActionButton;
@property(nonatomic, strong) UIButton* tertiaryActionButton;

@property(nonatomic, assign) BOOL didReachBottom;

// YES if the primary button content can be updated (e.g., change the text
// label string) which corresponds to the moment where the layout reflects the
// latest updates.
@property(nonatomic, assign) BOOL canUpdatePrimaryButton;

@end

@implementation FirstRunScreenViewController

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Create a layout guide for the margin between the subtitle and the screen-
  // specific content. A layout guide is needed because the margin scales with
  // the view height.
  UILayoutGuide* subtitleMarginLayoutGuide = [[UILayoutGuide alloc] init];

  self.scrollContentView = [[UIView alloc] init];
  self.scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.scrollContentView addSubview:self.imageView];
  [self.scrollContentView addSubview:self.titleLabel];
  [self.scrollContentView addSubview:self.subtitleLabel];
  [self.view addLayoutGuide:subtitleMarginLayoutGuide];
  [self.scrollContentView addSubview:self.specificContentView];

  // Wrap everything except the action buttons in a scroll view, to support
  // dynamic types.
  [self.scrollView addSubview:self.scrollContentView];
  [self.view addSubview:self.scrollView];

  UIStackView* actionStackView = [[UIStackView alloc] init];
  actionStackView.alignment = UIStackViewAlignmentFill;
  actionStackView.axis = UILayoutConstraintAxisVertical;
  actionStackView.translatesAutoresizingMaskIntoConstraints = NO;
  if (self.tertiaryActionString) {
    [actionStackView addArrangedSubview:self.tertiaryActionButton];
  }
  [actionStackView addArrangedSubview:self.primaryActionButton];
  if (self.secondaryActionString) {
    [actionStackView addArrangedSubview:self.secondaryActionButton];
  }
  [self.view addSubview:actionStackView];

  // Create a layout guide to constrain the width of the content, while still
  // allowing the scroll view to take the full screen width.
  UILayoutGuide* widthLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:widthLayoutGuide];

  CGFloat actionStackViewTopMargin = 0.0;
  if (!self.tertiaryActionString) {
    actionStackViewTopMargin = -kDefaultMargin;
  }

  CGFloat extraBottomMargin =
      (self.secondaryActionString || self.tertiaryActionString)
          ? 0
          : kActionsBottomMargin;

  [NSLayoutConstraint activateConstraints:@[
    // Content width layout guide constraints. Constrain the width to both at
    // least 65% of the view width, and to the full view width with margins.
    // This is to accomodate the iPad layout, which cannot be isolated out using
    // the traitCollection because of the FormSheet presentation style
    // (iPad FormSheet is considered compact).
    [widthLayoutGuide.centerXAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerXAnchor],
    [widthLayoutGuide.widthAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                 .widthAnchor
                                  multiplier:kContentWidthMultiplier],
    [widthLayoutGuide.widthAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .widthAnchor
                                 constant:-2 * kDefaultMargin],

    // Scroll view constraints.
    [self.scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.scrollView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.scrollView.bottomAnchor
        constraintEqualToAnchor:actionStackView.topAnchor
                       constant:actionStackViewTopMargin],

    // Scroll content view constraints. Constrain its height to at least the
    // scroll view height, so that derived VCs can pin UI elements just above
    // the buttons.
    [self.scrollContentView.topAnchor
        constraintEqualToAnchor:self.scrollView.topAnchor],
    [self.scrollContentView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [self.scrollContentView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],
    [self.scrollContentView.bottomAnchor
        constraintEqualToAnchor:self.scrollView.bottomAnchor],
    [self.scrollContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.scrollView.heightAnchor],

    // Banner image constraints. Scale the image vertically so its height takes
    // a certain % of the view height while maintaining its aspect ratio. Don't
    // constrain the width so that the image extends all the way to the edges of
    // the view, outside the scrollContentView.
    [self.imageView.topAnchor
        constraintEqualToAnchor:self.scrollContentView.topAnchor],
    [self.imageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],

    // Labels contraints. Attach them to the top of the scroll content view, and
    // center them horizontally.
    [self.titleLabel.topAnchor
        constraintEqualToAnchor:self.imageView.bottomAnchor],
    [self.titleLabel.centerXAnchor
        constraintEqualToAnchor:self.scrollContentView.centerXAnchor],
    [self.titleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.scrollContentView.widthAnchor],
    [self.subtitleLabel.topAnchor
        constraintEqualToAnchor:self.titleLabel.bottomAnchor
                       constant:kDefaultMargin],
    [self.subtitleLabel.centerXAnchor
        constraintEqualToAnchor:self.scrollContentView.centerXAnchor],
    [self.subtitleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.scrollContentView.widthAnchor],

    // Constraints for the screen-specific content view. It should take the
    // remaining scroll view area, with some margins on the top and sides.
    [subtitleMarginLayoutGuide.topAnchor
        constraintEqualToAnchor:self.subtitleLabel.bottomAnchor],
    [subtitleMarginLayoutGuide.heightAnchor
        constraintEqualToConstant:kDefaultMargin],
    [self.specificContentView.topAnchor
        constraintEqualToAnchor:subtitleMarginLayoutGuide.bottomAnchor],
    [self.specificContentView.leadingAnchor
        constraintEqualToAnchor:self.scrollContentView.leadingAnchor],
    [self.specificContentView.trailingAnchor
        constraintEqualToAnchor:self.scrollContentView.trailingAnchor],
    [self.specificContentView.bottomAnchor
        constraintEqualToAnchor:self.scrollContentView.bottomAnchor],

    // Action stack view constraints. Constrain the bottom of the action stack
    // view to both the bottom of the screen and the bottom of the safe area, to
    // give a nice result whether the device has a physical home button or not.
    [actionStackView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [actionStackView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],
    [actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                 constant:-kActionsBottomMargin -
                                          extraBottomMargin],
    [actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor
                                 constant:-extraBottomMargin],
  ]];

  // Also constrain the width layout guide to a maximum constant, but at a lower
  // priority so that it only applies in compact screens.
  NSLayoutConstraint* contentLayoutGuideWidthConstraint =
      [widthLayoutGuide.widthAnchor constraintEqualToConstant:kContentMaxWidth];
  contentLayoutGuideWidthConstraint.priority = UILayoutPriorityRequired - 1;
  contentLayoutGuideWidthConstraint.active = YES;

  // Also constrain the bottom of the action stack view to the bottom of the
  // safe area, but with a lower priority, so that the action stack view is put
  // as close to the bottom as possible.
  NSLayoutConstraint* actionBottomConstraint = [actionStackView.bottomAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
  actionBottomConstraint.priority = UILayoutPriorityDefaultLow;
  actionBottomConstraint.active = YES;
}

- (void)viewWillDisappear:(BOOL)animated {
  self.canUpdatePrimaryButton = NO;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Reset |didReachBottom| to make sure that its value is correctly updated
  // to reflect the scrolling state when the view reappears and is refreshed
  // (e.g., when getting back from a full screen view that was hidding this
  // view controller underneath).
  //
  // Set |didReachBottom| to YES when |scrollToEndMandatory| is NO, since the
  // screen can already be considered as fully scrolled when scrolling to the
  // end isn't mandatory.
  self.didReachBottom = !self.scrollToEndMandatory;

  // Only add the scroll view delegate after all the view layouts are fully
  // done.
  dispatch_async(dispatch_get_main_queue(), ^{
    self.scrollView.delegate = self;
    self.canUpdatePrimaryButton = YES;

    // At this point, the scroll view has computed its content height. If
    // scrolling to the end is needed, and the entire content is already
    // fully visible (scrolled), set |didReachBottom| to YES. Otherwise, replace
    // the primary button's label with the read more label to indicate that more
    // scrolling is required.
    if (!self.didReachBottom) {
      if ([self isScrolledToBottom]) {
        self.didReachBottom = YES;
      } else {
        [self setReadMoreText];
      }
    }
  });
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  // Update the primary button once the layout changes take effect to have the
  // right measurements to evaluate the scroll position.
  void (^transition)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self updatePrimaryButtonIfReachedBottom];
      };
  [coordinator animateAlongsideTransition:transition completion:nil];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  // Reset the title font and the learn more text to make sure that they are
  // properly scaled. Nothing will be done for the Read More text if the
  // bottom is reached.
  [self setTitleFont:self.titleLabel];
  [self setReadMoreText];

  // Update the primary button once the layout changes take effect to have the
  // right measurements to evaluate the scroll position.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self updatePrimaryButtonIfReachedBottom];
  });
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.imageView.image = [self scaleImage:self.imageView.image
                                   toSize:[self computeBannerImageSize]];
}

#pragma mark - Setter

- (void)setPrimaryActionString:(NSString*)text {
  _primaryActionString = text;
  // Change the button's label, unless scrolling to the end is mandatory and the
  // scroll view hasn't been scrolled to the end at least once yet.
  if (_primaryActionButton &&
      (!self.scrollToEndMandatory || self.didReachBottom)) {
    [_primaryActionButton setAttributedTitle:nil forState:UIControlStateNormal];
    [_primaryActionButton setTitle:_primaryActionString
                          forState:UIControlStateNormal];
  }
}

#pragma mark - Private

- (UIScrollView*)scrollView {
  if (!_scrollView) {
    _scrollView = [[UIScrollView alloc] init];
    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrollView.accessibilityIdentifier =
        kFirstRunScrollViewAccessibilityIdentifier;
  }
  return _scrollView;
}

- (UIImageView*)imageView {
  if (!_imageView) {
    _imageView = [[UIImageView alloc] initWithImage:self.bannerImage];
    _imageView.image = [self scaleImage:_imageView.image
                                 toSize:[self computeBannerImageSize]];
    _imageView.clipsToBounds = YES;
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _imageView;
}

// Computes banner's image size.
- (CGSize)computeBannerImageSize {
  CGFloat bannerMultiplier =
      self.isTallBanner ? kTallBannerMultiplier : kDefaultBannerMultiplier;

  CGFloat destinationHeight =
      roundf(self.view.bounds.size.height * bannerMultiplier);
  CGFloat destinationWidth =
      roundf(self.imageView.image.size.width /
             self.imageView.image.size.height * destinationHeight);
  CGSize newSize = CGSizeMake(destinationWidth, destinationHeight);
  return newSize;
}

// Returns a new UIImage which is |image| resized to |newSize|.
- (UIImage*)scaleImage:(UIImage*)image toSize:(CGSize)newSize {
  if (CGSizeEqualToSize(newSize, image.size)) {
    return image;
  }

  UIGraphicsBeginImageContextWithOptions(newSize, NO, 0.0);
  [image drawInRect:CGRectMake(0, 0, newSize.width, newSize.height)];
  UIImage* newImage = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return newImage;
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 0;
    [self setTitleFont:_titleLabel];
    UIFontDescriptor* descriptor = [UIFontDescriptor
        preferredFontDescriptorWithTextStyle:UIFontTextStyleLargeTitle];
    UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                     weight:UIFontWeightBold];
    UIFontMetrics* fontMetrics =
        [UIFontMetrics metricsForTextStyle:UIFontTextStyleLargeTitle];
    _titleLabel.font = [fontMetrics scaledFontForFont:font];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.text = self.titleText;
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.accessibilityIdentifier = kFirstRunTitleAccessibilityIdentifier;
  }
  return _titleLabel;
}

- (void)setTitleFont:(UILabel*)titleLabel {
  UIFontDescriptor* descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleLargeTitle];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* fontMetrics =
      [UIFontMetrics metricsForTextStyle:UIFontTextStyleLargeTitle];
  titleLabel.font = [fontMetrics scaledFontForFont:font];
}

- (UILabel*)subtitleLabel {
  if (!_subtitleLabel) {
    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _subtitleLabel.numberOfLines = 0;
    _subtitleLabel.textColor = [UIColor colorNamed:kGrey800Color];
    _subtitleLabel.text = self.subtitleText;
    _subtitleLabel.textAlignment = NSTextAlignmentCenter;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.accessibilityIdentifier =
        kFirstRunSubtitleAccessibilityIdentifier;
  }
  return _subtitleLabel;
}

- (UIView*)specificContentView {
  if (!_specificContentView) {
    _specificContentView = [[UIView alloc] init];
    _specificContentView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _specificContentView;
}

- (UIButton*)primaryActionButton {
  if (!_primaryActionButton) {
    _primaryActionButton = [[HighlightedButton alloc] initWithFrame:CGRectZero];
    _primaryActionButton.contentEdgeInsets =
        UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
    [_primaryActionButton setBackgroundColor:[UIColor colorNamed:kBlueColor]];
    UIColor* titleColor = [UIColor colorNamed:kSolidButtonTextColor];
    [_primaryActionButton setTitleColor:titleColor
                               forState:UIControlStateNormal];
    [self setPrimaryActionButtonFont:_primaryActionButton];
    _primaryActionButton.layer.cornerRadius = kPrimaryButtonCornerRadius;
    _primaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;

    if (@available(iOS 13.4, *)) {
      _primaryActionButton.pointerInteractionEnabled = YES;
      _primaryActionButton.pointerStyleProvider =
          CreateOpaqueButtonPointerStyleProvider();
    }

    // Use |primaryActionString| even if scrolling to the end is mandatory
    // because at the viewDidLoad stage, the scroll view hasn't computed its
    // content height, so there is no way to know if scrolling is needed. This
    // label will be updated at the viewDidAppear stage if necessary.
    [_primaryActionButton setTitle:self.primaryActionString
                          forState:UIControlStateNormal];
    _primaryActionButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    _primaryActionButton.accessibilityIdentifier =
        kFirstRunPrimaryActionAccessibilityIdentifier;
    _primaryActionButton.titleEdgeInsets =
        UIEdgeInsetsMake(0, kMoreArrowMargin, 0, kMoreArrowMargin);
    _primaryActionButton.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    [_primaryActionButton addTarget:self
                             action:@selector(didTapPrimaryActionButton)
                   forControlEvents:UIControlEventTouchUpInside];
  }
  return _primaryActionButton;
}

- (void)setPrimaryActionButtonFont:(UIButton*)button {
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
}

// Sets or resets the "Read More" text label when the bottom hasn't been
// reached yet and scrolling to the end is mandatory.
- (void)setReadMoreText {
  if (!self.scrollToEndMandatory) {
    return;
  }

  if (self.didReachBottom) {
    return;
  }

  if (!self.canUpdatePrimaryButton) {
    return;
  }

  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kSolidButtonTextColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
  };

  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_FIRST_RUN_SCREEN_READ_MORE)
              attributes:textAttributes];

  // Use |ceilf()| when calculating the icon's bounds to ensure the
  // button's content height does not shrink by fractional points, as the
  // attributed string's actual height is slightly smaller than the
  // assigned height.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  attachment.image = [[UIImage imageNamed:@"read_more_arrow"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  CGFloat height = ceilf(attributedString.size.height);
  CGFloat capHeight = ceilf(
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline].capHeight);
  CGFloat horizontalOffset =
      base::i18n::IsRTL() ? -1.f * kMoreArrowMargin : kMoreArrowMargin;
  CGFloat verticalOffset = (capHeight - height) / 2.f;
  attachment.bounds =
      CGRectMake(horizontalOffset, verticalOffset, height, height);
  [attributedString
      appendAttributedString:[NSAttributedString
                                 attributedStringWithAttachment:attachment]];

  // Make the title change without animation, as the UIButton's default
  // animation when using setTitle:forState: doesn't handle adding a
  // UIImage well (the old title gets abruptly pushed to the side as it's
  // fading out to make room for the new image, which looks awkward).
  __weak FirstRunScreenViewController* weakSelf = self;
  [UIView performWithoutAnimation:^{
    [weakSelf.primaryActionButton setAttributedTitle:attributedString
                                            forState:UIControlStateNormal];
    [weakSelf.primaryActionButton layoutIfNeeded];
  }];
}

- (UIButton*)secondaryActionButton {
  if (!_secondaryActionButton) {
    DCHECK(self.secondaryActionString);
    _secondaryActionButton =
        [self createButtonWithText:self.secondaryActionString
            accessibilityIdentifier:
                kFirstRunSecondaryActionAccessibilityIdentifier];
    [_secondaryActionButton addTarget:self
                               action:@selector(didTapSecondaryActionButton)
                     forControlEvents:UIControlEventTouchUpInside];
  }

  return _secondaryActionButton;
}

- (UIButton*)tertiaryActionButton {
  if (!_tertiaryActionButton) {
    DCHECK(self.tertiaryActionString);
    _tertiaryActionButton = [self
           createButtonWithText:self.tertiaryActionString
        accessibilityIdentifier:kFirstRunTertiaryActionAccessibilityIdentifier];
    [_tertiaryActionButton addTarget:self
                              action:@selector(didTapTertiaryActionButton)
                    forControlEvents:UIControlEventTouchUpInside];
  }

  return _tertiaryActionButton;
}

- (UIButton*)createButtonWithText:(NSString*)buttonText
          accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitle:buttonText forState:UIControlStateNormal];
  button.contentEdgeInsets =
      UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  [button setBackgroundColor:[UIColor clearColor]];
  UIColor* titleColor = [UIColor colorNamed:kBlueColor];
  [button setTitleColor:titleColor forState:UIControlStateNormal];
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.accessibilityIdentifier = accessibilityIdentifier;
  button.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;

  if (@available(iOS 13.4, *)) {
    button.pointerInteractionEnabled = YES;
    button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();
  }

  return button;
}

// Returns whether the scroll view's offset has reached the scroll view's
// content height, indicating that the scroll view has been fully scrolled.
- (BOOL)isScrolledToBottom {
  CGFloat scrollPosition =
      self.scrollView.contentOffset.y + self.scrollView.frame.size.height;
  CGFloat scrollLimit =
      self.scrollView.contentSize.height + self.scrollView.contentInset.bottom;
  return scrollPosition >= scrollLimit;
}

// If scrolling to the end of the content is mandatory, this method updates the
// primary button's label based on whether the scroll view is currently scrolled
// to the end. If the scroll view has scrolled to the end, also sets
// |didReachBottom|. If scrolling to the end of the content isn't mandatory, or
// if the scroll view had already been scrolled to the end previously, this
// method has no effect.
- (void)updatePrimaryButtonIfReachedBottom {
  if (!self.canUpdatePrimaryButton) {
    return;
  }

  if (self.scrollToEndMandatory && !self.didReachBottom &&
      [self isScrolledToBottom]) {
    self.didReachBottom = YES;
    [self.primaryActionButton setAttributedTitle:nil
                                        forState:UIControlStateNormal];
    [self.primaryActionButton setTitle:self.primaryActionString
                              forState:UIControlStateNormal];
    // Reset the font to make sure it is properly scaled.
    [self setPrimaryActionButtonFont:self.primaryActionButton];
  }
}

- (void)didTapPrimaryActionButton {
  if (self.scrollToEndMandatory && !self.didReachBottom) {
    // Calculate the offset needed to see the next content while keeping the
    // current content partially visible.
    CGFloat currentOffsetY = self.scrollView.contentOffset.y;
    CGPoint targetOffset = CGPointMake(
        0, currentOffsetY + self.scrollView.bounds.size.height *
                                (1.0 - kPreviousContentVisibleOnScroll));
    // Calculate the maximum possible offset. Add one point to work around some
    // issues when the fonts are increased.
    CGPoint bottomOffset =
        CGPointMake(0, self.scrollView.contentSize.height -
                           self.scrollView.bounds.size.height +
                           self.scrollView.contentInset.bottom + 1);
    // Scroll to the smaller of the two offsets.
    CGPoint newOffset =
        targetOffset.y < bottomOffset.y ? targetOffset : bottomOffset;
    [self.scrollView setContentOffset:newOffset animated:YES];
  } else if ([self.delegate
                 respondsToSelector:@selector(didTapPrimaryActionButton)]) {
    [self.delegate didTapPrimaryActionButton];
  }
}

- (void)didTapSecondaryActionButton {
  DCHECK(self.secondaryActionString);
  if ([self.delegate
          respondsToSelector:@selector(didTapSecondaryActionButton)]) {
    [self.delegate didTapSecondaryActionButton];
  }
}

- (void)didTapTertiaryActionButton {
  DCHECK(self.tertiaryActionString);
  if ([self.delegate
          respondsToSelector:@selector(didTapTertiaryActionButton)]) {
    [self.delegate didTapTertiaryActionButton];
  }
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updatePrimaryButtonIfReachedBottom];
}

@end
