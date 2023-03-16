// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/i18n/rtl.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat kDefaultMargin = 16;
// Default margin between the subtitle and the content view.
constexpr CGFloat kDefaultSubtitleBottomMargin = 22;
// Top margin for AvatarFullImageView in percentage of the dialog size.
constexpr CGFloat kAvatarFullImageViewTopMarginPercentage = 0.04;
constexpr CGFloat kFullAvatarImagerBottomMargin = 5;
constexpr CGFloat kTitleHorizontalMargin = 18;
constexpr CGFloat kActionsBottomMargin = 10;
constexpr CGFloat kTallBannerMultiplier = 0.35;
constexpr CGFloat kDefaultBannerMultiplier = 0.25;
constexpr CGFloat kContentWidthMultiplier = 0.8;
constexpr CGFloat kContentOptimalWidth = 327;
constexpr CGFloat kMoreArrowMargin = 4;
constexpr CGFloat kPreviousContentVisibleOnScroll = 0.15;
constexpr CGFloat kSeparatorHeight = 1;
constexpr CGFloat kLearnMoreButtonSide = 40;
constexpr CGFloat kAvatarImageSize = 48;
constexpr CGFloat kFullAvatarImageSize = 100;

}  // namespace

@interface PromoStyleViewController () <UIScrollViewDelegate>

// Whether banner is light or dark mode
@property(nonatomic, assign) UIUserInterfaceStyle bannerStyle;

@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) UIImageView* imageView;
// UIView that wraps the scrollable content.
@property(nonatomic, strong) UIView* scrollContentView;
// This view contains the avatar image with a shadow background image behind.
@property(nonatomic, strong) UIView* fullAvatarImageView;
// This view contains only the avatar image.
@property(nonatomic, strong) UIImageView* avatarImageView;
@property(nonatomic, strong) UILabel* subtitleLabel;
@property(nonatomic, strong) UITextView* disclaimerView;
@property(nonatomic, strong) UIStackView* actionStackView;
@property(nonatomic, strong) HighlightButton* primaryActionButton;
@property(nonatomic, strong) UIButton* secondaryActionButton;
@property(nonatomic, strong) UIButton* tertiaryActionButton;

// Layout constraint for `fullAvatarImageView` top margin.
@property(nonatomic, strong) NSLayoutConstraint* avatarFullImageViewTopMargin;

@property(nonatomic, strong) UIView* separator;
@property(nonatomic, assign) CGFloat scrollViewBottomOffsetY;

// Read/Write override.
@property(nonatomic, assign, readwrite) BOOL didReachBottom;

// YES if the views can be updated on scroll updates (e.g., change the text
// label string of the primary button) which corresponds to the moment where the
// layout reflects the latest updates.
@property(nonatomic, assign) BOOL canUpdateViewsOnScroll;

// Whether the image is currently being calculated; used to prevent infinite
// recursions caused by `viewDidLayoutSubviews`.
@property(nonatomic, assign) BOOL calculatingImageSize;

// Vertical constraints for buttons; used to reset top anchors when the number
// of buttons changes on scroll.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* buttonsVerticalAnchorConstraints;

// Vertical constraints for banner; used to deactivate these constraints when
// the banner is hidden.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* bannerConstraints;

@end

@implementation PromoStyleViewController

@synthesize learnMoreButton = _learnMoreButton;

#pragma mark - Public

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
  if (self) {
    _titleHorizontalMargin = kTitleHorizontalMargin;
    _subtitleBottomMargin = kDefaultSubtitleBottomMargin;
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Create a layout guide for the margin between the subtitle and the screen-
  // specific content. A layout guide is needed because the margin scales with
  // the view height.
  UILayoutGuide* subtitleMarginLayoutGuide = [[UILayoutGuide alloc] init];

  self.separator = [[UIView alloc] init];
  self.bannerStyle = UIUserInterfaceStyleUnspecified;
  self.separator.translatesAutoresizingMaskIntoConstraints = NO;
  self.separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  self.separator.hidden = YES;
  [self.view addSubview:self.separator];

  self.scrollContentView = [[UIView alloc] init];
  self.scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.scrollContentView addSubview:self.imageView];
  if (self.hasAvatarImage) {
    [self.scrollContentView addSubview:self.fullAvatarImageView];
    [self.fullAvatarImageView addSubview:self.avatarImageView];
  }
  [self.scrollContentView addSubview:self.titleLabel];
  [self.scrollContentView addSubview:self.subtitleLabel];
  [self.view addLayoutGuide:subtitleMarginLayoutGuide];
  [self.scrollContentView addSubview:self.specificContentView];
  if (self.disclaimerView) {
    [self.scrollContentView addSubview:self.disclaimerView];
  }

  // Wrap everything except the action buttons in a scroll view, to support
  // dynamic types.
  [self.scrollView addSubview:self.scrollContentView];
  [self.view addSubview:self.scrollView];

  // Add learn more button to top left of the view, if requested
  if (self.shouldShowLearnMoreButton) {
    [self.view insertSubview:self.learnMoreButton aboveSubview:self.scrollView];
  }

  self.actionStackView = [[UIStackView alloc] init];
  self.actionStackView.alignment = UIStackViewAlignmentFill;
  self.actionStackView.axis = UILayoutConstraintAxisVertical;
  self.actionStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.actionStackView addArrangedSubview:self.primaryActionButton];
  [self.view addSubview:self.actionStackView];

  // Create a layout guide to constrain the width of the content, while still
  // allowing the scroll view to take the full screen width.
  UILayoutGuide* widthLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:widthLayoutGuide];

  if (self.disclaimerView) {
    [NSLayoutConstraint activateConstraints:@[
      [self.disclaimerView.topAnchor
          constraintEqualToAnchor:self.specificContentView.bottomAnchor
                         constant:kDefaultMargin],
      [self.disclaimerView.leadingAnchor
          constraintEqualToAnchor:self.scrollContentView.leadingAnchor],
      [self.disclaimerView.trailingAnchor
          constraintEqualToAnchor:self.scrollContentView.trailingAnchor],
      [self.disclaimerView.bottomAnchor
          constraintEqualToAnchor:self.scrollContentView.bottomAnchor],
    ]];
  } else {
    [self.scrollContentView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor]
        .active = YES;
  }

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
    [self.scrollView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [self.scrollView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],

    // Separator constraints.
    [self.separator.heightAnchor constraintEqualToConstant:kSeparatorHeight],
    [self.separator.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.separator.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.separator.topAnchor
        constraintEqualToAnchor:self.scrollView.bottomAnchor],

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

    // Labels contraints. Attach them to the top of the scroll content view, and
    // center them horizontally.
    [self.titleLabel.centerXAnchor
        constraintEqualToAnchor:self.scrollContentView.centerXAnchor],
    [self.titleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.scrollContentView.widthAnchor
                                 constant:-2 * self.titleHorizontalMargin],
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
        constraintEqualToConstant:_subtitleBottomMargin],
    [self.specificContentView.topAnchor
        constraintEqualToAnchor:subtitleMarginLayoutGuide.bottomAnchor],
    [self.specificContentView.leadingAnchor
        constraintEqualToAnchor:self.scrollContentView.leadingAnchor],
    [self.specificContentView.trailingAnchor
        constraintEqualToAnchor:self.scrollContentView.trailingAnchor],

    // Action stack view constraints. Constrain the bottom of the action stack
    // view to both the bottom of the screen and the bottom of the safe area, to
    // give a nice result whether the device has a physical home button or not.
    [self.actionStackView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [self.actionStackView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],
  ]];

  if (self.hasAvatarImage) {
    self.avatarFullImageViewTopMargin = [self.fullAvatarImageView.topAnchor
        constraintEqualToAnchor:self.imageView.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
      self.avatarFullImageViewTopMargin,
      [self.titleLabel.topAnchor
          constraintEqualToAnchor:self.fullAvatarImageView.bottomAnchor
                         constant:kFullAvatarImagerBottomMargin],
      [self.fullAvatarImageView.centerXAnchor
          constraintEqualToAnchor:self.scrollContentView.centerXAnchor],
      [self.fullAvatarImageView.centerXAnchor
          constraintEqualToAnchor:self.avatarImageView.centerXAnchor],
      [self.fullAvatarImageView.centerYAnchor
          constraintEqualToAnchor:self.avatarImageView.centerYAnchor],
      [self.fullAvatarImageView.widthAnchor
          constraintEqualToConstant:kFullAvatarImageSize],
      [self.fullAvatarImageView.heightAnchor
          constraintEqualToConstant:kFullAvatarImageSize],
      [self.avatarImageView.widthAnchor
          constraintEqualToConstant:kAvatarImageSize],
      [self.avatarImageView.heightAnchor
          constraintEqualToConstant:kAvatarImageSize],
    ]];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [self.titleLabel.topAnchor
          constraintEqualToAnchor:self.imageView.bottomAnchor],
    ]];
  }

  [self setupBannerConstraints];

  self.buttonsVerticalAnchorConstraints = @[
    [self.scrollView.bottomAnchor
        constraintEqualToAnchor:self.actionStackView.topAnchor
                       constant:-kDefaultMargin],
    [self.actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                 constant:-kActionsBottomMargin * 2],
    [self.actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor
                                 constant:-kActionsBottomMargin],
  ];
  [NSLayoutConstraint
      activateConstraints:self.buttonsVerticalAnchorConstraints];

  // This constraint is added to enforce that the content width should be as
  // close to the optimal width as possible, within the range already activated
  // for "widthLayoutGuide.widthAnchor" previously, with a higher priority.
  // In this case, the content width in iPad and iPhone landscape mode should be
  // the safe layout width multiplied by kContentWidthMultiplier, while the
  // content width for a iPhone portrait mode should be kContentOptimalWidth.
  NSLayoutConstraint* contentLayoutGuideWidthConstraint =
      [widthLayoutGuide.widthAnchor
          constraintEqualToConstant:kContentOptimalWidth];
  contentLayoutGuideWidthConstraint.priority = UILayoutPriorityRequired - 1;
  contentLayoutGuideWidthConstraint.active = YES;

  // Also constrain the bottom of the action stack view to the bottom of the
  // safe area, but with a lower priority, so that the action stack view is put
  // as close to the bottom as possible.
  NSLayoutConstraint* actionBottomConstraint =
      [self.actionStackView.bottomAnchor
          constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
  actionBottomConstraint.priority = UILayoutPriorityDefaultLow;
  actionBottomConstraint.active = YES;

  if (self.shouldShowLearnMoreButton) {
    [NSLayoutConstraint activateConstraints:@[
      [self.learnMoreButton.topAnchor
          constraintEqualToAnchor:self.scrollContentView.topAnchor],
      [self.learnMoreButton.leadingAnchor
          constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
      [self.learnMoreButton.widthAnchor
          constraintEqualToConstant:kLearnMoreButtonSide],
      [self.learnMoreButton.heightAnchor
          constraintEqualToConstant:kLearnMoreButtonSide],
    ]];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  self.canUpdateViewsOnScroll = NO;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Reset `didReachBottom` to make sure that its value is correctly updated
  // to reflect the scrolling state when the view reappears and is refreshed
  // (e.g., when getting back from a full screen view that was hidding this
  // view controller underneath).
  //
  // Set `didReachBottom` to YES when `scrollToEndMandatory` is NO, since the
  // screen can already be considered as fully scrolled when scrolling to the
  // end isn't mandatory.
  self.didReachBottom = !self.scrollToEndMandatory;

  // Only add the scroll view delegate after all the view layouts are fully
  // done.
  dispatch_async(dispatch_get_main_queue(), ^{
    self.scrollView.delegate = self;
    self.canUpdateViewsOnScroll = YES;

    // At this point, the scroll view has computed its content height. If
    // scrolling to the end is needed, and the entire content is already
    // fully visible (scrolled), set `didReachBottom` to YES. Otherwise, replace
    // the primary button's label with the read more label to indicate that more
    // scrolling is required.
    self.scrollViewBottomOffsetY = self.scrollView.contentSize.height -
                                   self.scrollView.bounds.size.height +
                                   self.scrollView.contentInset.bottom;

    BOOL isScrolledToBottom = [self isScrolledToBottom];
    self.separator.hidden = isScrolledToBottom;
    if (self.didReachBottom || isScrolledToBottom) {
      [self updateActionButtonsAndPushUpScrollViewIfMandatory];
    } else {
      [self setReadMoreText];
    }
  });
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // Prevents potential recursive calls to `viewDidLayoutSubviews`.
  if (self.calculatingImageSize) {
    return;
  }
  // Rescale image here as on iPad the view height isn't correctly set before
  // subviews are laid out.
  self.calculatingImageSize = YES;
  self.imageView.image =
      [self scaleBannerWithCurrentImage:self.imageView.image
                                 toSize:[self computeBannerImageSize]];
  self.calculatingImageSize = NO;
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  // Update the buttons once the layout changes take effect to have the right
  // measurements to evaluate the scroll position.
  void (^transition)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self updateViewsOnScrollViewUpdate];
      };
  [coordinator animateAlongsideTransition:transition completion:nil];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  self.avatarFullImageViewTopMargin.constant = AlignValueToPixel(
      self.view.bounds.size.height * kAvatarFullImageViewTopMarginPercentage);
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  // Reset the title font and the learn more text to make sure that they are
  // properly scaled. Nothing will be done for the Read More text if the
  // bottom is reached.
  [self setFontForTitle:self.titleLabel];
  [self setReadMoreText];

  // Update the primary button once the layout changes take effect to have the
  // right measurements to evaluate the scroll position.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self updateViewsOnScrollViewUpdate];
  });
}

#pragma mark - Accessors

- (void)setShouldBannerFillTopSpace:(BOOL)shouldBannerFillTopSpace {
  _shouldBannerFillTopSpace = shouldBannerFillTopSpace;
  [self setupBannerConstraints];
  self.imageView.image =
      [self scaleBannerWithCurrentImage:self.imageView.image
                                 toSize:[self computeBannerImageSize]];
}

- (void)setShouldHideBanner:(BOOL)shouldHideBanner {
  _shouldHideBanner = shouldHideBanner;
  [self setupBannerConstraints];
  self.imageView.image =
      [self scaleBannerWithCurrentImage:self.imageView.image
                                 toSize:[self computeBannerImageSize]];
}

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

- (UIScrollView*)scrollView {
  if (!_scrollView) {
    _scrollView = [[UIScrollView alloc] init];
    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrollView.accessibilityIdentifier =
        kPromoStyleScrollViewAccessibilityIdentifier;
  }
  return _scrollView;
}

- (UIImageView*)imageView {
  if (!_imageView) {
    _imageView = [[UIImageView alloc]
        initWithImage:
            [self scaleBannerWithCurrentImage:nil
                                       toSize:[self computeBannerImageSize]]];
    _imageView.clipsToBounds = YES;
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _imageView;
}

- (UIView*)fullAvatarImageView {
  if (!_fullAvatarImageView) {
    DCHECK(self.hasAvatarImage);
    UIImage* circleImage = [UIImage imageNamed:@"promo_style_avatar_circle"];
    DCHECK(circleImage);
    _fullAvatarImageView = [[UIImageView alloc] initWithImage:circleImage];
    _fullAvatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _fullAvatarImageView;
}

- (UIImageView*)avatarImageView {
  if (!_avatarImageView) {
    DCHECK(self.hasAvatarImage);
    _avatarImageView = [[UIImageView alloc] initWithImage:self.avatarImage];
    _avatarImageView.clipsToBounds = YES;
    _avatarImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _avatarImageView.layer.cornerRadius = kAvatarImageSize / 2.;
  }
  return _avatarImageView;
}

- (void)setAvatarImage:(UIImage*)avatarImage {
  _avatarImage = avatarImage;
  if (self.hasAvatarImage) {
    DCHECK_EQ(avatarImage.size.width, kAvatarImageSize);
    DCHECK_EQ(avatarImage.size.height, kAvatarImageSize);
    self.avatarImageView.image = avatarImage;
  }
}

- (void)setAvatarAccessibilityLabel:(NSString*)avatarAccessibilityLabel {
  _avatarAccessibilityLabel = avatarAccessibilityLabel;
  if (self.hasAvatarImage) {
    self.avatarImageView.accessibilityLabel = avatarAccessibilityLabel;
    self.avatarImageView.isAccessibilityElement =
        avatarAccessibilityLabel != nil;
  }
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 0;
    [self setFontForTitle:_titleLabel];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.text = self.titleText;
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.accessibilityIdentifier =
        kPromoStyleTitleAccessibilityIdentifier;
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
    _subtitleLabel.textColor = [UIColor colorNamed:kGrey800Color];
    _subtitleLabel.text = self.subtitleText;
    _subtitleLabel.textAlignment = NSTextAlignmentCenter;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.accessibilityIdentifier =
        kPromoStyleSubtitleAccessibilityIdentifier;
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
    _primaryActionButton = [[HighlightButton alloc] initWithFrame:CGRectZero];

    // TODO(crbug.com/1418068): Replace with UIButtonConfiguration when min
    // deployment target is iOS 15.
    UIEdgeInsets contentInsets =
        UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
    SetContentEdgeInsets(_primaryActionButton, contentInsets);
    UIEdgeInsets titleInsets =
        UIEdgeInsetsMake(0, kMoreArrowMargin, 0, kMoreArrowMargin);
    SetTitleEdgeInsets(_primaryActionButton, titleInsets);

    [_primaryActionButton setBackgroundColor:[UIColor colorNamed:kBlueColor]];
    UIColor* titleColor = [UIColor colorNamed:kSolidButtonTextColor];
    [_primaryActionButton setTitleColor:titleColor
                               forState:UIControlStateNormal];
    [self setPrimaryActionButtonFont:_primaryActionButton];
    _primaryActionButton.layer.cornerRadius = kPrimaryButtonCornerRadius;
    _primaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;

    _primaryActionButton.pointerInteractionEnabled = YES;
    _primaryActionButton.pointerStyleProvider =
        CreateOpaqueButtonPointerStyleProvider();

    // Use `primaryActionString` even if scrolling to the end is mandatory
    // because at the viewDidLoad stage, the scroll view hasn't computed its
    // content height, so there is no way to knOow if scrolling is needed.
    // This label will be updated at the viewDidAppear stage if necessary.
    [_primaryActionButton setTitle:self.primaryActionString
                          forState:UIControlStateNormal];
    UILabel* titleLabel = _primaryActionButton.titleLabel;
    titleLabel.adjustsFontSizeToFitWidth = YES;
    titleLabel.minimumScaleFactor = 0.7;
    _primaryActionButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    _primaryActionButton.accessibilityIdentifier =
        kPromoStylePrimaryActionAccessibilityIdentifier;
    _primaryActionButton.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    [_primaryActionButton addTarget:self
                             action:@selector(didTapPrimaryActionButton)
                   forControlEvents:UIControlEventTouchUpInside];
  }
  return _primaryActionButton;
}

- (UITextView*)disclaimerView {
  if (!self.disclaimerText) {
    return nil;
  }
  if (!_disclaimerView) {
    // Set up disclaimer view.
    _disclaimerView = CreateUITextViewWithTextKit1();
    _disclaimerView.accessibilityIdentifier =
        kPromoStyleDisclaimerViewAccessibilityIdentifier;
    _disclaimerView.textContainerInset = UIEdgeInsetsMake(0, 0, 0, 0);
    _disclaimerView.scrollEnabled = NO;
    _disclaimerView.editable = NO;
    _disclaimerView.adjustsFontForContentSizeCategory = YES;
    _disclaimerView.delegate = self;
    _disclaimerView.backgroundColor = UIColor.clearColor;
    _disclaimerView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    _disclaimerView.translatesAutoresizingMaskIntoConstraints = NO;
    _disclaimerView.attributedText = [self attributedStringForDisclaimer];
  }
  return _disclaimerView;
}

- (void)setDisclaimerText:(NSString*)disclaimerText {
  _disclaimerText = disclaimerText;
  NSAttributedString* attributedText = [self attributedStringForDisclaimer];
  if (attributedText) {
    self.disclaimerView.attributedText = attributedText;
  }
}

- (void)setDisclaimerURLs:(NSArray<NSURL*>*)disclaimerURLs {
  _disclaimerURLs = disclaimerURLs;
  NSAttributedString* attributedText = [self attributedStringForDisclaimer];
  if (attributedText) {
    self.disclaimerView.attributedText = attributedText;
  }
}

- (UIButton*)secondaryActionButton {
  if (!_secondaryActionButton) {
    DCHECK(self.secondaryActionString);
    _secondaryActionButton =
        [self createButtonWithText:self.secondaryActionString
            accessibilityIdentifier:
                kPromoStyleSecondaryActionAccessibilityIdentifier];
    UILabel* titleLabel = _secondaryActionButton.titleLabel;
    titleLabel.adjustsFontSizeToFitWidth = YES;
    titleLabel.minimumScaleFactor = 0.7;

    [_secondaryActionButton addTarget:self
                               action:@selector(didTapSecondaryActionButton)
                     forControlEvents:UIControlEventTouchUpInside];
  }

  return _secondaryActionButton;
}

- (UIButton*)tertiaryActionButton {
  if (!_tertiaryActionButton) {
    DCHECK(self.tertiaryActionString);
    _tertiaryActionButton =
        [self createButtonWithText:self.tertiaryActionString
            accessibilityIdentifier:
                kPromoStyleTertiaryActionAccessibilityIdentifier];
    [_tertiaryActionButton addTarget:self
                              action:@selector(didTapTertiaryActionButton)
                    forControlEvents:UIControlEventTouchUpInside];
  }

  return _tertiaryActionButton;
}

// Helper to create the learn more button.
- (UIButton*)learnMoreButton {
  if (!_learnMoreButton) {
    DCHECK(self.shouldShowLearnMoreButton);
    _learnMoreButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_learnMoreButton setImage:[UIImage imageNamed:@"help_icon"]
                      forState:UIControlStateNormal];
    _learnMoreButton.accessibilityIdentifier =
        kPromoStyleLearnMoreActionAccessibilityIdentifier;
    _learnMoreButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_learnMoreButton addTarget:self
                         action:@selector(didTapLearnMoreButton)
               forControlEvents:UIControlEventTouchUpInside];
  }
  return _learnMoreButton;
}

#pragma mark - Private

// Updates banner constraints.
- (void)setupBannerConstraints {
  if (self.scrollContentView == nil) {
    return;
  }

  if (self.bannerConstraints != nil) {
    [NSLayoutConstraint deactivateConstraints:self.bannerConstraints];
  }

  self.bannerConstraints = @[
    // Common banner image constraints, further constraints are added below.
    // This one ensures the banner is well centered within the view.
    [self.imageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
  ];

  if (self.shouldHideBanner) {
    self.bannerConstraints =
        [self.bannerConstraints arrayByAddingObjectsFromArray:@[
          [self.imageView.heightAnchor constraintEqualToConstant:0],
          [self.imageView.topAnchor
              constraintEqualToAnchor:self.scrollContentView.topAnchor
                             constant:kDefaultMargin]
        ]];
  } else if (self.shouldBannerFillTopSpace) {
    NSLayoutDimension* dimFromToOfViewToBottomOfBanner = [self.view.topAnchor
        anchorWithOffsetToAnchor:self.imageView.bottomAnchor];
    // Constrain bottom of banner to top of view + C * height of view
    // where C = isTallBanner ? tallMultiplier : defaultMultiplier.
    self.bannerConstraints =
        [self.bannerConstraints arrayByAddingObjectsFromArray:@[
          [dimFromToOfViewToBottomOfBanner
              constraintEqualToAnchor:self.view.heightAnchor
                           multiplier:self.isTallBanner
                                          ? kTallBannerMultiplier
                                          : kDefaultBannerMultiplier]
        ]];
  } else {
    // Default.
    self.bannerConstraints =
        [self.bannerConstraints arrayByAddingObjectsFromArray:@[
          [self.imageView.topAnchor
              constraintEqualToAnchor:self.scrollContentView.topAnchor],
        ]];
  }

  [NSLayoutConstraint activateConstraints:self.bannerConstraints];
}

- (UIImage*)bannerImage {
  if (self.shouldHideBanner && !self.bannerName) {
    return [[UIImage alloc] init];
  }
  return [UIImage imageNamed:self.bannerName];
}

// Computes banner's image size.
- (CGSize)computeBannerImageSize {
  if (self.shouldHideBanner) {
    return CGSizeZero;
  }
  CGFloat bannerMultiplier =
      self.isTallBanner ? kTallBannerMultiplier : kDefaultBannerMultiplier;
  CGFloat bannerAspectRatio =
      [self bannerImage].size.width / [self bannerImage].size.height;

  CGFloat destinationHeight = 0;
  CGFloat destinationWidth = 0;

  if (!self.shouldBannerFillTopSpace) {
    destinationHeight = roundf(self.view.bounds.size.height * bannerMultiplier);
    destinationWidth = roundf(bannerAspectRatio * destinationHeight);
  } else {
    CGFloat minBannerWidth = self.view.bounds.size.width;
    CGFloat minBannerHeight = self.view.bounds.size.height * bannerMultiplier;
    destinationWidth =
        roundf(fmax(minBannerWidth, bannerAspectRatio * minBannerHeight));
    destinationHeight = roundf(bannerAspectRatio * destinationWidth);
  }

  CGSize newSize = CGSizeMake(destinationWidth, destinationHeight);
  return newSize;
}

// Returns a new UIImage which is `sourceImage` resized to `newSize`. Returns
// `currentImage` if it is already at the correct size.
- (UIImage*)scaleBannerWithCurrentImage:(UIImage*)currentImage
                                 toSize:(CGSize)newSize {
  UIUserInterfaceStyle currentStyle =
      UITraitCollection.currentTraitCollection.userInterfaceStyle;
  if (CGSizeEqualToSize(newSize, currentImage.size) &&
      self.bannerStyle == currentStyle) {
    return currentImage;
  }

  self.bannerStyle = currentStyle;
  return ResizeImage([self bannerImage], newSize, ProjectionMode::kAspectFit);
}

// Determines which font text style to use depending on the device size, the
// size class and if dynamic type is enabled.
- (UIFontTextStyle)titleLabelFontTextStyle {
  UIViewController* presenter =
      self.presentingViewController ? self.presentingViewController : self;
  BOOL dynamicTypeEnabled = UIContentSizeCategoryIsAccessibilityCategory(
      presenter.traitCollection.preferredContentSizeCategory);

  if (!dynamicTypeEnabled) {
    if ([self isRegularXRegularSizeClass:presenter.traitCollection]) {
      return UIFontTextStyleTitle1;
    } else if (!IsSmallDevice()) {
      return UIFontTextStyleLargeTitle;
    }
  }
  return UIFontTextStyleTitle2;
}

- (void)setFontForTitle:(UILabel*)titleLabel {
  UIFontTextStyle textStyle = [self titleLabelFontTextStyle];

  UIFontDescriptor* descriptor =
      [UIFontDescriptor preferredFontDescriptorWithTextStyle:textStyle];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* fontMetrics = [UIFontMetrics metricsForTextStyle:textStyle];
  titleLabel.font = [fontMetrics scaledFontForFont:font];
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

  if (!self.canUpdateViewsOnScroll) {
    return;
  }

  DCHECK(self.readMoreString);
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kSolidButtonTextColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
  };

  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:self.readMoreString
                                             attributes:textAttributes];

  // Use `ceilf()` when calculating the icon's bounds to ensure the
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
  self.primaryActionButton.accessibilityIdentifier =
      kPromoStyleReadMoreActionAccessibilityIdentifier;

  // Make the title change without animation, as the UIButton's default
  // animation when using setTitle:forState: doesn't handle adding a
  // UIImage well (the old title gets abruptly pushed to the side as it's
  // fading out to make room for the new image, which looks awkward).
  __weak PromoStyleViewController* weakSelf = self;
  [UIView performWithoutAnimation:^{
    [weakSelf.primaryActionButton setAttributedTitle:attributedString
                                            forState:UIControlStateNormal];
    [weakSelf.primaryActionButton layoutIfNeeded];
  }];
}

- (UIButton*)createButtonWithText:(NSString*)buttonText
          accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitle:buttonText forState:UIControlStateNormal];
  [button setBackgroundColor:[UIColor clearColor]];
  UIColor* titleColor = [UIColor colorNamed:kBlueColor];
  [button setTitleColor:titleColor forState:UIControlStateNormal];

  // TODO(crbug.com/1418068): Replace with UIButtonConfiguration when min
  // deployment target is iOS 15.
  UIEdgeInsets contentInsets =
      UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  SetContentEdgeInsets(button, contentInsets);

  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.accessibilityIdentifier = accessibilityIdentifier;
  button.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

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
// action buttons based on whether the scroll view is currently scrolled to the
// end. If the scroll view has scrolled to the end, also sets `didReachBottom`.
// It also updates the separator visibility based on scroll position.
- (void)updateViewsOnScrollViewUpdate {
  if (!self.canUpdateViewsOnScroll) {
    return;
  }

  BOOL isScrolledToBottom = [self isScrolledToBottom];
  self.separator.hidden = isScrolledToBottom;
  if (self.scrollToEndMandatory && !self.didReachBottom && isScrolledToBottom) {
    [self updateActionButtonsAndPushUpScrollViewIfMandatory];
  }
}

// This method should be called right before the view is scrolled to the bottom.
// It updates the primary button's label and adds secondary and/or tertiary
// buttons, and as a result, pushing the scroll view up by updating the bottom
// offset of the scroll view and scroll to the new offset if the change in
// action buttons is triggered by a scroll in a view that sets
// `self.scrollToEndMandatory=YES`. It also sets `self.didReachBottom` to YES.
- (void)updateActionButtonsAndPushUpScrollViewIfMandatory {
  [self.primaryActionButton setAttributedTitle:nil
                                      forState:UIControlStateNormal];
  [self.primaryActionButton setTitle:self.primaryActionString
                            forState:UIControlStateNormal];
  self.primaryActionButton.accessibilityIdentifier =
      kPromoStylePrimaryActionAccessibilityIdentifier;
  // Reset the font to make sure it is properly scaled.
  [self setPrimaryActionButtonFont:self.primaryActionButton];

  CGFloat originalHeightForActionStack =
      self.primaryActionButton.bounds.size.height;
  for (NSLayoutConstraint* constraint in self
           .buttonsVerticalAnchorConstraints) {
    originalHeightForActionStack += constraint.constant;
  }
  CGFloat newEstimatedHeightForActionStack =
      self.primaryActionButton.bounds.size.height;
  // Add other buttons with the correct margins.
  if (self.secondaryActionString) {
    [self.actionStackView insertArrangedSubview:self.secondaryActionButton
                                        atIndex:1];
    newEstimatedHeightForActionStack +=
        self.secondaryActionButton.intrinsicContentSize.height;
  }
  if (self.tertiaryActionString) {
    [self.actionStackView insertArrangedSubview:self.tertiaryActionButton
                                        atIndex:0];
    newEstimatedHeightForActionStack +=
        self.tertiaryActionButton.intrinsicContentSize.height;
  }
  for (NSLayoutConstraint* constraint in self
           .buttonsVerticalAnchorConstraints) {
    newEstimatedHeightForActionStack += constraint.constant;
  }
  CGFloat estimatedHeightDifference =
      newEstimatedHeightForActionStack - originalHeightForActionStack;
  CGPoint updatedBottomOffset =
      CGPointMake(0, self.scrollViewBottomOffsetY + estimatedHeightDifference);

  if (self.secondaryActionString || self.tertiaryActionString) {
    // Update constraints.
    [NSLayoutConstraint
        deactivateConstraints:self.buttonsVerticalAnchorConstraints];
    self.buttonsVerticalAnchorConstraints = @[
      [self.scrollView.bottomAnchor
          constraintEqualToAnchor:self.actionStackView.topAnchor
                         constant:self.tertiaryActionString ? 0
                                                            : -kDefaultMargin],
      [self.actionStackView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                   constant:-kActionsBottomMargin],
      [self.actionStackView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                .bottomAnchor],
    ];
    [NSLayoutConstraint
        activateConstraints:self.buttonsVerticalAnchorConstraints];
  }
  if (self.scrollToEndMandatory && ![self isScrolledToBottom]) {
    [self.scrollView setContentOffset:updatedBottomOffset animated:YES];
  }
  self.didReachBottom = YES;
}

- (void)didTapPrimaryActionButton {
  if (self.scrollToEndMandatory && !self.didReachBottom) {
    // Calculate the offset needed to see the next content while keeping the
    // current content partially visible.
    CGFloat currentOffsetY = self.scrollView.contentOffset.y;
    CGPoint targetOffset = CGPointMake(
        0, currentOffsetY + self.scrollView.bounds.size.height *
                                (1.0 - kPreviousContentVisibleOnScroll));
    // Add one point to maximum possible offset to work around some issues when
    // the fonts are increased.
    if (targetOffset.y < self.scrollViewBottomOffsetY + 1) {
      [self.scrollView setContentOffset:targetOffset animated:YES];
    } else {
      [self updateActionButtonsAndPushUpScrollViewIfMandatory];
    }
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

// Handle taps on the help button.
- (void)didTapLearnMoreButton {
  DCHECK(self.shouldShowLearnMoreButton);
  if ([self.delegate respondsToSelector:@selector(didTapLearnMoreButton)]) {
    [self.delegate didTapLearnMoreButton];
  }
}

// Helper that returns whether the `traitCollection` has a regular vertical
// and regular horizontal size class.
// Copied from "ios/chrome/browser/shared/ui/util/uikit_ui_util.mm"
- (bool)isRegularXRegularSizeClass:(UITraitCollection*)traitCollection {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

// Helper class that returns the an NSAttributedString generated from the
// current disclaimer text and URLs.
- (NSAttributedString*)attributedStringForDisclaimer {
  StringWithTags parsedString = ParseStringWithLinks(self.disclaimerText);
  if (parsedString.ranges.size() != [self.disclaimerURLs count]) {
    return nil;
  }

  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string
                                             attributes:textAttributes];
  size_t index = 0;
  for (NSURL* url in self.disclaimerURLs) {
    [attributedText addAttribute:NSLinkAttributeName
                           value:url
                           range:parsedString.ranges[index]];
    index += 1;
  }
  return attributedText;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateViewsOnScrollViewUpdate];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (textView == self.disclaimerView &&
      [self.delegate respondsToSelector:@selector(didTapURLInDisclaimer:)]) {
    [self.delegate didTapURLInDisclaimer:URL];
  }
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the `selectedTextRange` to `nil` to prevent users from
  // selecting text. Setting the `selectable` property to `NO` doesn't help
  // since it makes links inside the text view untappable. Another solution is
  // to subclass `UITextView` and override `canBecomeFirstResponder` to return
  // NO, but that workaround only works on iOS 13.5+. This is the simplest
  // approach that works well on iOS 12, 13 & 14.
  textView.selectedTextRange = nil;
}

@end
