// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/i18n/rtl.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "ios/chrome/common/app_group/app_group_utils.h"
#import "ios/chrome/common/constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/promo_style/promo_style_background_view.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Default margin between the subtitle and the content view.
constexpr CGFloat kDefaultSubtitleBottomMargin = 22;
// Top margin for no background header image in percentage of the dialog size.
constexpr CGFloat kNoBackgroundHeaderImageTopMarginPercentage = 0.04;
constexpr CGFloat kNoBackgroundHeaderImageBottomMargin = 5;
// Top margin for header image with background in percentage of the dialog size.
constexpr CGFloat kHeaderImageBackgroundTopMarginPercentage = 0.1;
constexpr CGFloat kHeaderImageBackgroundBottomMargin = 34;
constexpr CGFloat kTitleHorizontalMargin = 18;
constexpr CGFloat kTitleNoHeaderTopMargin = 56;
constexpr CGFloat kTallBannerMultiplier = 0.35;
constexpr CGFloat kExtraTallBannerMultiplier = 0.5;
constexpr CGFloat kDefaultBannerMultiplier = 0.25;
constexpr CGFloat kShortBannerMultiplier = 0.2;
constexpr CGFloat kExtraShortBannerMultiplier = 0.15;
constexpr CGFloat kLearnMoreButtonSide = 40;
constexpr CGFloat kheaderImageSize = 48;
constexpr CGFloat kFullheaderImageSize = 100;
constexpr CGFloat kButtonPadding = 8;
constexpr CGFloat kReadMoreImagePadding = 8;

// Corner radius for the whole view.
constexpr CGFloat kCornerRadius = 20;

// Duration for the buttons' fade-in animation.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(200);

// Properties of the header image kImageWithShadow type shadow.
const CGFloat kHeaderImageCornerRadius = 13;
const CGFloat kHeaderImageShadowOffsetX = 0;
const CGFloat kHeaderImageShadowOffsetY = 0;
const CGFloat kHeaderImageShadowRadius = 6;
const CGFloat kHeaderImageShadowOpacity = 0.1;
const CGFloat kHeaderImageShadowShadowInset = 20;

// Returns the arrow down image with the correct configuration.
UIImage* ArrowDownImage() {
  UIImageSymbolConfiguration* symbol_configuration = [UIImageSymbolConfiguration
      configurationWithTextStyle:UIFontTextStyleCaption2];
  symbol_configuration = [symbol_configuration
      configurationByApplyingConfiguration:
          [UIImageSymbolConfiguration
              configurationWithWeight:UIImageSymbolWeightBold]];
  return [UIImage systemImageNamed:@"arrow.down"
                 withConfiguration:symbol_configuration];
}

}  // namespace

@interface PromoStyleViewController () <ButtonStackActionDelegate,
                                        UIScrollViewDelegate>

@property(nonatomic, strong) UIImageView* bannerImageView;
// This view contains only the header image.
@property(nonatomic, strong) UIImageView* headerImageView;
@property(nonatomic, strong) UITextView* disclaimerView;
// Read/Write override.
@property(nonatomic, assign, readwrite) BOOL didReachBottom;

@end

@implementation PromoStyleViewController {
  // This view contains the header image with a shadow background image behind.
  UIView* _fullHeaderImageView;
  // This view contains the background image for the header image. The header
  // view will be placed at the center of it.
  UIImageView* _headerBackgroundImageView;

  // Layout constraint for `headerBackgroundImageView` top margin.
  NSLayoutConstraint* _headerBackgroundImageViewTopMargin;
  // Layout constraint for `titleLabel` top margin when there is no banner or
  // header.
  NSLayoutConstraint* _titleLabelNoHeaderTopMargin;
  // Layout guide for the margin between the subtitle and the screen-specific
  // content.
  UILayoutGuide* _subtitleMarginLayoutGuide;
  // Vertical constraints for banner; used to deactivate these constraints when
  // the banner is hidden.
  NSArray<NSLayoutConstraint*>* _bannerConstraints;

  // Whether banner is light or dark mode.
  UIUserInterfaceStyle _bannerStyle;
  // YES if the views can be updated on scroll updates (e.g., change the text
  // label string of the primary button) which corresponds to the moment where
  // the layout reflects the latest updates.
  BOOL _canUpdateViewsOnScroll;
  // Whether the image is currently being calculated; used to prevent infinite
  // recursions caused by `viewDidLayoutSubviews`.
  BOOL _calculatingImageSize;
  // Indicate that the view should scroll to the bottom at the end of the next
  // layout.
  BOOL _shouldScrollToBottom;
  // Task runner to resize banner image off the UI thread.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;
  // Backup of the primary action string to restore it after "Read More" state.
  NSString* _originalPrimaryActionString;
}

@synthesize actionButtonsVisibility = _actionButtonsVisibility;
@synthesize dismissButton = _dismissButton;
@synthesize learnMoreButton = _learnMoreButton;

#pragma mark - Public

- (instancetype)initWithTaskRunner:
    (scoped_refptr<base::SequencedTaskRunner>)taskRunner {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  self = [super initWithConfiguration:configuration];
  if (self) {
    self.actionDelegate = self;
    _titleHorizontalMargin = kTitleHorizontalMargin;
    _subtitleBottomMargin = kDefaultSubtitleBottomMargin;
    _headerImageShadowInset = kHeaderImageShadowShadowInset;
    _headerImageBottomMargin = kButtonStackMargin;
    _noBackgroundHeaderImageTopMarginPercentage =
        kNoBackgroundHeaderImageTopMarginPercentage;
    _taskRunner = taskRunner;
    _bannerStyle = UIUserInterfaceStyleUnspecified;
  }

  return self;
}

- (instancetype)init {
  scoped_refptr<base::SequencedTaskRunner> taskRunner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  return [self initWithTaskRunner:taskRunner];
}

- (UIFontTextStyle)titleLabelFontTextStyle {
  // Determine which font text style to use depending on the device size, the
  // size class and if dynamic type is enabled.
  return GetTitleLabelFontTextStyle(self);
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  [self updateButtonStyles];

  UIView* view = self.view;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  [self setupBackgroundView];
  [self.contentView addSubview:self.bannerImageView];
  [self setupHeaderView];
  [self setupLabels];
  [self setupContentViews];
  [self setupActionButtons];

  UITextView* disclaimerView = self.disclaimerView;
  UIView* specificContentView = self.specificContentView;
  if (disclaimerView) {
    [NSLayoutConstraint activateConstraints:@[
      [disclaimerView.topAnchor
          constraintEqualToAnchor:specificContentView.bottomAnchor
                         constant:kButtonStackMargin],
      [disclaimerView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [disclaimerView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],

    ]];
    if (self.topAlignedLayout) {
      [NSLayoutConstraint activateConstraints:@[
        [disclaimerView.bottomAnchor
            constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor]
      ]];
    } else {
      [NSLayoutConstraint activateConstraints:@[
        [disclaimerView.bottomAnchor
            constraintEqualToAnchor:self.contentView.bottomAnchor]
      ]];
    }
  } else {
    [self.contentView.bottomAnchor
        constraintEqualToAnchor:specificContentView.bottomAnchor]
        .active = YES;
  }

  if (self.preferToCompressContent) {
    // To make the content unscrollable, constrain the height of the content
    // view. Set constraint priority to UILayoutPriorityDefaultLow + 1 so that
    // this constraint is deactivated and content is made scrollable only after
    // views with compression resistance of UILayoutPriorityDefaultLow are
    // first compressed.
    NSLayoutConstraint* contentViewUnscrollableHeightConstraint =
        [self.contentView.heightAnchor
            constraintEqualToAnchor:self.view.heightAnchor];
    contentViewUnscrollableHeightConstraint.priority =
        UILayoutPriorityDefaultLow + 1;
    contentViewUnscrollableHeightConstraint.active = YES;
  }

  [NSLayoutConstraint activateConstraints:@[
    // Labels contraints. Attach them to the top of the scroll content view, and
    // center them horizontally.
    [self.titleLabel.centerXAnchor
        constraintEqualToAnchor:self.contentView.centerXAnchor],
    [self.titleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.widthAnchor
                                 constant:-2 * self.titleHorizontalMargin],
    [_subtitleLabel.topAnchor
        constraintEqualToAnchor:self.titleLabel.bottomAnchor
                       constant:kButtonStackMargin],
    [_subtitleLabel.centerXAnchor
        constraintEqualToAnchor:self.contentView.centerXAnchor],
    [_subtitleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.widthAnchor],

    // Constraints for the screen-specific content view. It should take the
    // remaining scroll view area, with some margins on the top and sides.
    [_subtitleMarginLayoutGuide.topAnchor
        constraintEqualToAnchor:_subtitleLabel.bottomAnchor],
    [_subtitleMarginLayoutGuide.heightAnchor
        constraintEqualToConstant:_subtitleBottomMargin],

    [specificContentView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor],
    [specificContentView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor],
  ]];

  if (self.hideSpecificContentView) {
    // Hide the specificContentView and do not add the margin.
    [NSLayoutConstraint activateConstraints:@[
      [specificContentView.topAnchor
          constraintEqualToAnchor:_subtitleLabel.bottomAnchor],
      [specificContentView.heightAnchor constraintEqualToConstant:0]
    ]];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [specificContentView.topAnchor
          constraintEqualToAnchor:_subtitleMarginLayoutGuide.bottomAnchor]
    ]];
  }

  if (self.bannerLimitWithRoundedCorner) {
    // Add a subview with the same background of the view and put it over the
    // banner image view.
    UIView* limitView = [[UIView alloc] init];
    limitView.clipsToBounds = YES;
    limitView.translatesAutoresizingMaskIntoConstraints = NO;
    limitView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    [self.contentView insertSubview:limitView
                       aboveSubview:self.bannerImageView];

    // Corner radius cannot reach over half of the view, so set the height to 2*
    // kCornerRadius.
    [NSLayoutConstraint activateConstraints:@[
      [limitView.centerYAnchor
          constraintEqualToAnchor:_bannerImageView.bottomAnchor],
      [limitView.leftAnchor constraintEqualToAnchor:self.view.leftAnchor],
      [limitView.rightAnchor constraintEqualToAnchor:self.view.rightAnchor],
      [limitView.heightAnchor constraintEqualToConstant:2 * kCornerRadius],
    ]];
    limitView.layer.cornerRadius = kCornerRadius;
    limitView.layer.maskedCorners =
        kCALayerMaxXMinYCorner | kCALayerMinXMinYCorner;
    limitView.layer.masksToBounds = true;
  }

  if (self.headerImageType != PromoStyleImageType::kNone) {
    _headerBackgroundImageViewTopMargin = [_headerBackgroundImageView.topAnchor
        constraintEqualToAnchor:self.bannerImageView.bottomAnchor];
    CGFloat headerImageBottomMargin = _headerImageBottomMargin;
    if (self.headerImageType == PromoStyleImageType::kAvatar) {
      headerImageBottomMargin = self.headerBackgroundImage == nil
                                    ? kNoBackgroundHeaderImageBottomMargin
                                    : kHeaderImageBackgroundBottomMargin;
    }
    UIImageView* headerImageView = self.headerImageView;
    [NSLayoutConstraint activateConstraints:@[
      _headerBackgroundImageViewTopMargin,
      [_titleLabel.topAnchor
          constraintEqualToAnchor:_headerBackgroundImageView.bottomAnchor
                         constant:headerImageBottomMargin],
      [_headerBackgroundImageView.centerXAnchor
          constraintEqualToAnchor:self.contentView.centerXAnchor],
      [_headerBackgroundImageView.centerXAnchor
          constraintEqualToAnchor:_fullHeaderImageView.centerXAnchor],
      [_headerBackgroundImageView.centerYAnchor
          constraintEqualToAnchor:_fullHeaderImageView.centerYAnchor],
      [_fullHeaderImageView.centerXAnchor
          constraintEqualToAnchor:headerImageView.centerXAnchor],
      [_fullHeaderImageView.centerYAnchor
          constraintEqualToAnchor:headerImageView.centerYAnchor],
    ]];
    if (self.headerImageType == PromoStyleImageType::kAvatar) {
      [NSLayoutConstraint activateConstraints:@[
        [_fullHeaderImageView.widthAnchor
            constraintEqualToConstant:kFullheaderImageSize],
        [_fullHeaderImageView.heightAnchor
            constraintEqualToConstant:kFullheaderImageSize],
        [_headerBackgroundImageView.widthAnchor
            constraintGreaterThanOrEqualToConstant:kFullheaderImageSize],
        [_headerBackgroundImageView.heightAnchor
            constraintGreaterThanOrEqualToConstant:kFullheaderImageSize],
        [headerImageView.widthAnchor
            constraintEqualToConstant:kheaderImageSize],
        [headerImageView.heightAnchor
            constraintEqualToConstant:kheaderImageSize],
      ]];
    }
    if (self.headerImageType == PromoStyleImageType::kImage ||
        self.headerImageType == PromoStyleImageType::kImageWithShadow) {
      [NSLayoutConstraint activateConstraints:@[
        [_fullHeaderImageView.widthAnchor
            constraintEqualToAnchor:_fullHeaderImageView.heightAnchor],
        [_fullHeaderImageView.widthAnchor
            constraintGreaterThanOrEqualToAnchor:headerImageView.widthAnchor
                                        constant:_headerImageShadowInset],
        [_fullHeaderImageView.heightAnchor
            constraintGreaterThanOrEqualToAnchor:headerImageView.heightAnchor
                                        constant:_headerImageShadowInset],

        [_headerBackgroundImageView.widthAnchor
            constraintGreaterThanOrEqualToAnchor:_fullHeaderImageView
                                                     .widthAnchor],
        [_headerBackgroundImageView.heightAnchor
            constraintGreaterThanOrEqualToAnchor:_fullHeaderImageView
                                                     .heightAnchor],
      ]];
      // Set low priority constraint to set the width/height according to image
      // size. If image ratio is not 1:1, this will conflict with the
      // height = width constraint abve and one of the 2 will be dropped.
      NSLayoutConstraint* widthConstraint = [_fullHeaderImageView.widthAnchor
          constraintEqualToAnchor:headerImageView.widthAnchor
                         constant:_headerImageShadowInset];
      widthConstraint.priority = UILayoutPriorityDefaultLow;
      widthConstraint.active = YES;
      NSLayoutConstraint* heightConstraint = [_fullHeaderImageView.heightAnchor
          constraintEqualToAnchor:headerImageView.heightAnchor
                         constant:_headerImageShadowInset];
      heightConstraint.priority = UILayoutPriorityDefaultLow;
      heightConstraint.active = YES;
    }
    [_headerBackgroundImageView
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];
    [_headerBackgroundImageView
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisVertical];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [_titleLabel.topAnchor
          constraintEqualToAnchor:self.bannerImageView.bottomAnchor
                         constant:_titleTopMarginWhenNoHeaderImage],
    ]];
  }

  [self setupBannerConstraints];

  if (self.shouldShowLearnMoreButton) {
    UIButton* learnMoreButton = self.learnMoreButton;
    [NSLayoutConstraint activateConstraints:@[
      [learnMoreButton.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor],
      [learnMoreButton.leadingAnchor
          constraintEqualToAnchor:view.safeAreaLayoutGuide.leadingAnchor],
      [learnMoreButton.widthAnchor
          constraintEqualToConstant:kLearnMoreButtonSide],
      [learnMoreButton.heightAnchor
          constraintEqualToConstant:kLearnMoreButtonSide],
    ]];
  }

  if (self.shouldShowDismissButton) {
    [NSLayoutConstraint activateConstraints:@[
      [_dismissButton.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor],
      [_dismissButton.trailingAnchor
          constraintEqualToAnchor:view.safeAreaLayoutGuide.trailingAnchor
                         constant:-kButtonStackMargin],
    ]];

    // Align learn more and dismiss buttons vertically if both exist.
    if (self.shouldShowLearnMoreButton) {
      [NSLayoutConstraint activateConstraints:@[
        [_learnMoreButton.centerYAnchor
            constraintEqualToAnchor:self.dismissButton.centerYAnchor],
        [_learnMoreButton.trailingAnchor
            constraintLessThanOrEqualToAnchor:_dismissButton.leadingAnchor
                                     constant:-kButtonPadding],
      ]];
    }
  }

  if (self.hideHeaderOnTallContent) {
    [self handleDidReachBottomOfContent];
  }

  NSArray<UITrait>* traits = @[
    UITraitVerticalSizeClass.class, UITraitHorizontalSizeClass.class,
    UITraitPreferredContentSizeCategory.class
  ];
  [self registerForTraitChanges:traits
                     withAction:@selector(updateUIOnTraitChange)];
}

- (void)viewWillDisappear:(BOOL)animated {
  _canUpdateViewsOnScroll = NO;
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  if (self.isBeingDismissed &&
      [self.delegate respondsToSelector:@selector(didDismissViewController)]) {
    [self.delegate didDismissViewController];
  }
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Set `didReachBottom` to YES when `scrollToEndMandatory` is NO, since the
  // screen can already be considered as fully scrolled when scrolling to the
  // end isn't mandatory.
  if (!self.scrollToEndMandatory) {
    self.didReachBottom = YES;
  }

  // Update views based on the initial scroll state, once the layout is fully
  // done.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self updateViewsForInitialScrollState];
  });
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // Prevents potential recursive calls to `viewDidLayoutSubviews`.
  if (_calculatingImageSize) {
    return;
  }
  // Rescale image here as on iPad the view height isn't correctly set before
  // subviews are laid out.
  _calculatingImageSize = YES;
  [self scaleBannerWithCurrentImage:self.bannerImageView.image
                             toSize:[self computeBannerImageSize]];
  _calculatingImageSize = NO;
  if (_shouldScrollToBottom) {
    _shouldScrollToBottom = NO;
    dispatch_async(dispatch_get_main_queue(), ^{
      [self scrollToBottom];
    });
  }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  // Update the buttons once the layout changes take effect to have the right
  // measurements to evaluate the scroll position.
  __weak __typeof(self) weakSelf = self;
  void (^transition)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf updateViewsOnScrollViewUpdate];
        [weakSelf hideHeaderOnTallContentIfNeeded];
      };
  [coordinator animateAlongsideTransition:transition completion:nil];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  if (!self.topAlignedLayout) {
    CGFloat headerImageTopMarginPercentage =
        self.headerBackgroundImage == nil
            ? _noBackgroundHeaderImageTopMarginPercentage
            : kHeaderImageBackgroundTopMarginPercentage;
    _headerBackgroundImageViewTopMargin.constant = AlignValueToPixel(
        self.view.bounds.size.height * headerImageTopMarginPercentage);
  }
}

- (void)setActionButtonsVisibility:
    (ActionButtonsVisibility)actionButtonsVisibility {
  if (_actionButtonsVisibility == actionButtonsVisibility) {
    return;
  }
  _actionButtonsVisibility = actionButtonsVisibility;
  [self updateButtonStyles];

  // On hidden visibility, hide the entire button stack view and the disclaimer
  // view above it.
  if (actionButtonsVisibility == ActionButtonsVisibility::kHidden) {
    self.disclaimerView.hidden = YES;
    return;
  }

  // Fade the disclaimer text in if hidden.
  if (![self hasVisibleButtons]) {
    self.disclaimerView.alpha = 0;
    self.disclaimerView.hidden = NO;
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:kAnimationDuration.InSecondsF()
                     animations:^{
                       PromoStyleViewController* strongSelf = weakSelf;
                       if (!strongSelf) {
                         return;
                       }
                       strongSelf.disclaimerView.alpha = 1.0;
                     }
                     completion:nil];
  }
}

#pragma mark - Accessors

- (void)setShouldBannerFillTopSpace:(BOOL)shouldBannerFillTopSpace {
  _shouldBannerFillTopSpace = shouldBannerFillTopSpace;
  [self setupBannerConstraints];
  [self scaleBannerWithCurrentImage:self.bannerImageView.image
                             toSize:[self computeBannerImageSize]];
}

- (void)setShouldHideBanner:(BOOL)shouldHideBanner {
  _shouldHideBanner = shouldHideBanner;
  [self setupBannerConstraints];
  [self scaleBannerWithCurrentImage:self.bannerImageView.image
                             toSize:[self computeBannerImageSize]];
}

- (UIImageView*)bannerImageView {
  if (!_bannerImageView) {
    _bannerImageView = [[UIImageView alloc] init];
    [self scaleBannerWithCurrentImage:nil toSize:[self computeBannerImageSize]];
    _bannerImageView.clipsToBounds = YES;
    _bannerImageView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _bannerImageView;
}

- (UIImageView*)headerImageView {
  if (!_headerImageView) {
    DCHECK(self.headerImageType != PromoStyleImageType::kNone);
    _headerImageView = [[UIImageView alloc] initWithImage:self.headerImage];
    _headerImageView.clipsToBounds = YES;
    _headerImageView.translatesAutoresizingMaskIntoConstraints = NO;
    if (self.headerImageType == PromoStyleImageType::kAvatar) {
      _headerImageView.layer.cornerRadius = kheaderImageSize / 2.;
    }
    _headerImageView.image = _headerImage;
    _headerImageView.accessibilityLabel = _headerAccessibilityLabel;
    _headerImageView.isAccessibilityElement = _headerAccessibilityLabel != nil;
    if (self.headerViewForceStyleLight) {
      _headerImageView.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;
    }
  }
  return _headerImageView;
}

- (void)setHeaderImage:(UIImage*)headerImage {
  _headerImage = headerImage;
  if (self.headerImageType == PromoStyleImageType::kAvatar) {
    DCHECK_EQ(headerImage.size.width, kheaderImageSize);
    DCHECK_EQ(headerImage.size.height, kheaderImageSize);
  }
  // `self.headerImageView` should not be used to avoid creating the image.
  // The owner might set the image first and then change the value of
  // `self.headerImageType`.
  _headerImageView.image = headerImage;
}

- (void)setHeaderAccessibilityLabel:(NSString*)headerAccessibilityLabel {
  _headerAccessibilityLabel = headerAccessibilityLabel;
  // `self.headerImageView` should not be used to avoid creating the image.
  // The owner might set the accessibility label and then change the value of
  // `self.headerImageType`.
  _headerImageView.accessibilityLabel = headerAccessibilityLabel;
  _headerImageView.isAccessibilityElement = headerAccessibilityLabel != nil;
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 0;
    _titleLabel.font = GetFRETitleFont(self.titleLabelFontTextStyle);
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

- (UIView*)specificContentView {
  if (!_specificContentView) {
    _specificContentView = [[UIView alloc] init];
    _specificContentView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _specificContentView;
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

// Helper to create the dismiss button.
- (UIButton*)dismissButton {
  if (!_dismissButton) {
    CHECK(self.shouldShowDismissButton);
    CHECK(self.dismissButtonString);

    _dismissButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_dismissButton setTitle:self.dismissButtonString
                    forState:UIControlStateNormal];
    [_dismissButton addTarget:self
                       action:@selector(didTapDismissButton)
             forControlEvents:UIControlEventTouchUpInside];
    _dismissButton.translatesAutoresizingMaskIntoConstraints = NO;
    _dismissButton.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  }
  return _dismissButton;
}

#pragma mark - ButtonStackActionDelegate

- (void)didTapPrimaryActionButton {
  if (self.scrollToEndMandatory && !self.didReachBottom) {
    [self scrollToBottom];
    return;
  }
  if ([self.delegate respondsToSelector:@selector(didTapPrimaryActionButton)]) {
    base::UmaHistogramEnumeration("IOS.PromoStyleSheet.Outcome",
                                  PromoStyleSheetAction::kPrimaryButtonTapped);
    [self.delegate didTapPrimaryActionButton];
  }
}

- (void)didTapSecondaryActionButton {
  if ([self.delegate
          respondsToSelector:@selector(didTapSecondaryActionButton)]) {
    base::UmaHistogramEnumeration(
        "IOS.PromoStyleSheet.Outcome",
        PromoStyleSheetAction::kSecondaryButtonTapped);
    [self.delegate didTapSecondaryActionButton];
  }
}

- (void)didTapTertiaryActionButton {
  if ([self.delegate
          respondsToSelector:@selector(didTapTertiaryActionButton)]) {
    base::UmaHistogramEnumeration("IOS.PromoStyleSheet.Outcome",
                                  PromoStyleSheetAction::kTertiaryButtonTapped);
    [self.delegate didTapTertiaryActionButton];
  }
}

#pragma mark - Private

// Sets the button styles based on `actionButtonsVisibility`.
- (void)updateButtonStyles {
  switch (self.actionButtonsVisibility) {
    case ActionButtonsVisibility::kDefault:
    case ActionButtonsVisibility::kRegularButtonsShown:
      self.configuration.hideButtons = NO;
      self.configuration.primaryButtonStyle = ChromeButtonStylePrimary;
      self.configuration.secondaryButtonStyle = ChromeButtonStyleSecondary;
      self.configuration.tertiaryButtonStyle = ChromeButtonStyleTertiary;
      break;
    case ActionButtonsVisibility::kEquallyWeightedButtonShown:
      self.configuration.hideButtons = NO;
      self.configuration.primaryButtonStyle = ChromeButtonStyleTertiary;
      self.configuration.secondaryButtonStyle = ChromeButtonStyleTertiary;
      self.configuration.tertiaryButtonStyle = ChromeButtonStyleTertiary;
      break;
    case ActionButtonsVisibility::kHidden:
      self.configuration.hideButtons = YES;
      break;
  }
  [self reloadConfiguration];
}

// Creates and returns the full header image view based on the `headerImageType`
// property.
- (UIView*)createFullHeaderImageView {
  switch (self.headerImageType) {
    case PromoStyleImageType::kAvatar: {
      UIImage* circleImage = [UIImage imageNamed:@"promo_style_avatar_circle"];
      DCHECK(circleImage);
      UIImageView* imageView = [[UIImageView alloc] initWithImage:circleImage];
      imageView.translatesAutoresizingMaskIntoConstraints = NO;
      return imageView;
    }
    case PromoStyleImageType::kImageWithShadow: {
      UIView* frameView = [[UIView alloc] init];
      frameView.translatesAutoresizingMaskIntoConstraints = NO;
      frameView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
      if (self.headerViewForceStyleLight) {
        frameView.overrideUserInterfaceStyle = UIUserInterfaceStyleLight;
      }
      frameView.layer.cornerRadius = kHeaderImageCornerRadius;
      frameView.layer.shadowOffset =
          CGSizeMake(kHeaderImageShadowOffsetX, kHeaderImageShadowOffsetY);
      frameView.layer.shadowRadius = kHeaderImageShadowRadius;
      frameView.layer.shadowOpacity = kHeaderImageShadowOpacity;
      return frameView;
    }
    case PromoStyleImageType::kImage: {
      UIView* frameView = [[UIView alloc] init];
      frameView.translatesAutoresizingMaskIntoConstraints = NO;
      return frameView;
    }
    case PromoStyleImageType::kNone:
      NOTREACHED();
  }
}

// Creates and returns the header background image view.
- (UIImageView*)createHeaderBackgroundImageView {
  CHECK(self.headerImageType != PromoStyleImageType::kNone);
  UIImageView* imageView =
      [[UIImageView alloc] initWithImage:self.headerBackgroundImage];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.accessibilityIdentifier =
      kPromoStyleHeaderViewBackgroundAccessibilityIdentifier;
  return imageView;
}

// Creates and returns the subtitle label.
- (UILabel*)createSubtitleLabel {
  UILabel* subtitleLabel = [[UILabel alloc] init];
  subtitleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.textColor = [UIColor colorNamed:kGrey800Color];
  subtitleLabel.text = self.subtitleText;
  subtitleLabel.textAlignment = NSTextAlignmentCenter;
  subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  subtitleLabel.adjustsFontForContentSizeCategory = YES;
  subtitleLabel.accessibilityIdentifier =
      kPromoStyleSubtitleAccessibilityIdentifier;
  return subtitleLabel;
}

// Updates banner constraints.
- (void)setupBannerConstraints {
  if (self.contentView == nil) {
    return;
  }

  if (_bannerConstraints != nil) {
    [NSLayoutConstraint deactivateConstraints:_bannerConstraints];
  }

  _bannerConstraints = @[
    // Common banner image constraints, further constraints are added below.
    // This one ensures the banner is well centered within the content view.
    [self.bannerImageView.centerXAnchor
        constraintEqualToAnchor:self.contentView.centerXAnchor],
  ];

  // Default automatic content inset adjustment.
  UIScrollViewContentInsetAdjustmentBehavior contentInsetAdjustmentBehavior =
      UIScrollViewContentInsetAdjustmentAutomatic;

  if (self.shouldHideBanner) {
    _bannerConstraints = [_bannerConstraints arrayByAddingObjectsFromArray:@[
      [self.bannerImageView.heightAnchor constraintEqualToConstant:0],
      [self.bannerImageView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor]
    ]];
  } else if (self.shouldBannerFillTopSpace) {
    NSLayoutDimension* dimFromToOfViewToBottomOfBanner = [self.view.topAnchor
        anchorWithOffsetToAnchor:self.bannerImageView.bottomAnchor];
    // Constrain bottom of banner to top of view + C * height of view
    // where C = isTallBanner ? tallMultiplier : defaultMultiplier.
    _bannerConstraints = [_bannerConstraints arrayByAddingObjectsFromArray:@[
      [dimFromToOfViewToBottomOfBanner
          constraintEqualToAnchor:self.view.heightAnchor
                       multiplier:[self bannerMultiplier]]
    ]];
    // When the banner fills the top space, it should go behind the navigation
    // bar.
    contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
  } else {
    // Default.
    _bannerConstraints = [_bannerConstraints arrayByAddingObjectsFromArray:@[
      [self.bannerImageView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor],
    ]];
  }
  self.contentInsetAdjustmentBehavior = contentInsetAdjustmentBehavior;

  [NSLayoutConstraint activateConstraints:_bannerConstraints];
}

// Returns the banner image to be displayed.
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
  CGFloat bannerMultiplier = [self bannerMultiplier];
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

// Returns the multiplier for the banner height based on the `bannerSize`.
- (CGFloat)bannerMultiplier {
  switch (self.bannerSize) {
    case BannerImageSizeType::kExtraShort:
      return kExtraShortBannerMultiplier;
    case BannerImageSizeType::kShort:
      return kShortBannerMultiplier;
    case BannerImageSizeType::kStandard:
      return kDefaultBannerMultiplier;
    case BannerImageSizeType::kTall:
      return kTallBannerMultiplier;
    case BannerImageSizeType::kExtraTall:
      return kExtraTallBannerMultiplier;
  }
}

// Asynchronously updates `self.bannerImageView.image` to `[self bannerImage]`
// resized to `newSize`. If `currentImage` is already the correct size then
// `self.bannerImageView.image` is instead set to `currentImage` synchronously.
// If there is no task runner, then `self.bannerImageView.image` is updated
// synchronously.
- (void)scaleBannerWithCurrentImage:(UIImage*)currentImage
                             toSize:(CGSize)newSize {
  UIUserInterfaceStyle currentStyle =
      UITraitCollection.currentTraitCollection.userInterfaceStyle;
  if (CGSizeEqualToSize(newSize, currentImage.size) &&
      _bannerStyle == currentStyle) {
    self.bannerImageView.image = currentImage;
    return;
  }

  _bannerStyle = currentStyle;

  // Resize on the UI thread if there is no TaskRunner (this can happen in
  // application extensions).
  if (!_taskRunner) {
    self.bannerImageView.image =
        ResizeImage([self bannerImage], newSize, ProjectionMode::kAspectFit);
    return;
  }

  // Otherwise, resize image off the UI thread.
  _taskRunner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](UIImage* bannerImage, CGSize newSize) {
            return ResizeImage(bannerImage, newSize,
                               ProjectionMode::kAspectFit);
          },
          [self bannerImage], newSize),
      base::BindOnce(
          [](UIImageView* bannerImageView, UIImage* resizedBannerImage) {
            bannerImageView.image = resizedBannerImage;
          },
          self.bannerImageView));
}

// Sets or resets the "Read More" text label when the bottom has not been
// reached yet and scrolling to the end is mandatory.
- (void)setReadMoreText {
  if (!self.scrollToEndMandatory) {
    return;
  }

  if (self.didReachBottom) {
    return;
  }

  if (!_canUpdateViewsOnScroll) {
    return;
  }

  CHECK(self.readMoreString);
  if (!_originalPrimaryActionString) {
    _originalPrimaryActionString = self.configuration.primaryActionString;
  }
  self.configuration.primaryActionString = self.readMoreString;
  [self reloadConfiguration];

  UIButtonConfiguration* config = self.primaryActionButton.configuration;
  config.image = ArrowDownImage();
  config.imagePlacement = NSDirectionalRectEdgeTrailing;
  config.imagePadding = kReadMoreImagePadding;
  config.imageColorTransformer = ^UIColor*(UIColor* _) {
    return [UIColor colorNamed:kSolidButtonTextColor];
  };
  self.primaryActionButton.configuration = config;
}

// Updates views based on the initial scroll state. This should be
// done only once all the view layouts are fully done.
- (void)updateViewsForInitialScrollState {
  _canUpdateViewsOnScroll = YES;

  // At this point, the scroll view has computed its content height. If
  // scrolling to the end is needed, and the entire content is already
  // fully visible (scrolled), set `didReachBottom` to YES. Otherwise, replace
  // the primary button's label with the read more label to indicate that more
  // scrolling is required.
  BOOL isScrolledToBottom = [self isScrolledToBottom];
  if (self.didReachBottom || isScrolledToBottom) {
    [self handleDidReachBottomOfContent];
  } else {
    [self setReadMoreText];
  }
}

// If scrolling to the end of the content is mandatory, this method updates the
// action buttons based on whether the scroll view is currently scrolled to the
// end. If the scroll view has scrolled to the end, also sets `didReachBottom`.
// It also updates the separator visibility based on scroll position.
- (void)updateViewsOnScrollViewUpdate {
  if (!_canUpdateViewsOnScroll) {
    return;
  }

  BOOL isScrolledToBottom = [self isScrolledToBottom];
  if (self.scrollToEndMandatory && !self.didReachBottom && isScrolledToBottom) {
    [self handleDidReachBottomOfContent];
  }
}

// If scrolling to the end is mandatory, it triggers a scroll to the bottom.
// It also handles hiding the header if the content is tall and sets
// `self.didReachBottom` to YES.
- (void)handleDidReachBottomOfContent {
  if (self.scrollToEndMandatory) {
    _shouldScrollToBottom = YES;
  } else if (self.hideHeaderOnTallContent) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self hideHeaderOnTallContentIfNeeded];
    });
  }
  self.didReachBottom = YES;

  if (!_originalPrimaryActionString) {
    return;
  }

  self.configuration.primaryActionString = _originalPrimaryActionString;
  _originalPrimaryActionString = nil;
  [self reloadConfiguration];

  UIButtonConfiguration* config = self.primaryActionButton.configuration;
  config.image = nil;
  self.primaryActionButton.configuration = config;
}

// Handle taps on the help button.
- (void)didTapLearnMoreButton {
  DCHECK(self.shouldShowLearnMoreButton);
  if ([self.delegate respondsToSelector:@selector(didTapLearnMoreButton)]) {
    [self.delegate didTapLearnMoreButton];
  }
}

// Handle taps on the dismiss button.
- (void)didTapDismissButton {
  CHECK(self.shouldShowDismissButton);
  if ([self.delegate respondsToSelector:@selector(didTapDismissButton)]) {
    [self.delegate didTapDismissButton];
    base::UmaHistogramEnumeration("IOS.PromoStyleSheet.Outcome",
                                  PromoStyleSheetAction::kDismissButtonTapped);
  }
}

// Returns the font text style for the disclaimer label.
- (UIFontTextStyle)disclaimerLabelFontTextStyle {
  return UIFontTextStyleCaption2;
}

// Helper method that returns an NSAttributedString generated from the
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
        [UIFont preferredFontForTextStyle:[self disclaimerLabelFontTextStyle]],
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

// Hides the header if the content is tall and the header should be hidden.
- (void)hideHeaderOnTallContentIfNeeded {
  // Once hidden, the header will not reappear.
  if (!self.hideHeaderOnTallContent || !_canUpdateViewsOnScroll ||
      _fullHeaderImageView.hidden) {
    return;
  }
  CHECK(self.headerImageType != PromoStyleImageType::kNone);

  const BOOL contentFits = [self isScrolledToBottom];
  if (contentFits) {
    return;
  }

  _fullHeaderImageView.hidden = YES;
  _headerBackgroundImageView.hidden = YES;
  _headerImageView.hidden = YES;
  if (!_titleLabelNoHeaderTopMargin) {
    _titleLabelNoHeaderTopMargin = [_titleLabel.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:kTitleNoHeaderTopMargin];
  }
  _titleLabelNoHeaderTopMargin.active = YES;
  _headerBackgroundImageViewTopMargin.active = NO;

  [_headerBackgroundImageView layoutIfNeeded];
  [self updateViewsOnScrollViewUpdate];
}

// Sets up the background view.
- (void)setupBackgroundView {
  if (!self.usePromoStyleBackground) {
    return;
  }
  CHECK(self.shouldHideBanner);
  UIView* backgroundView = [[PromoStyleBackgroundView alloc] init];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  backgroundView.layer.zPosition = -1;
  [self.view insertSubview:backgroundView atIndex:0];
  AddSameConstraints(self.view, backgroundView);
}

// Sets up the header view.
- (void)setupHeaderView {
  if (self.headerImageType == PromoStyleImageType::kNone) {
    return;
  }
  _fullHeaderImageView = [self createFullHeaderImageView];
  _headerBackgroundImageView = [self createHeaderBackgroundImageView];
  [self.contentView addSubview:_headerBackgroundImageView];
  [_headerBackgroundImageView addSubview:_fullHeaderImageView];
  [_fullHeaderImageView addSubview:self.headerImageView];
}

// Sets up the title and subtitle labels.
- (void)setupLabels {
  UILabel* titleLabel = self.titleLabel;
  [self.contentView addSubview:titleLabel];
  _subtitleLabel = [self createSubtitleLabel];
  [self.contentView addSubview:_subtitleLabel];
  _subtitleMarginLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:_subtitleMarginLayoutGuide];
}

// Sets up the specific content view and disclaimer view.
- (void)setupContentViews {
  UIView* specificContentView = self.specificContentView;
  [self.contentView addSubview:specificContentView];

  UITextView* disclaimerView = self.disclaimerView;
  if (disclaimerView) {
    [self.contentView addSubview:disclaimerView];
  }
}

// Sets up the learn more and dismiss buttons.
- (void)setupActionButtons {
  // Add learn more button to top left of the view, if requested.
  if (self.shouldShowLearnMoreButton) {
    [self.view addSubview:self.learnMoreButton];
  }

  // Add dismiss button to top right of the view, if requested.
  if (self.shouldShowDismissButton) {
    [self.view addSubview:self.dismissButton];
  }
}

// Updates certain UI elements when changes in the device's UI traits have been
// observed.
- (void)updateUIOnTraitChange {
  // Reset the title font and the learn more text to make sure that they are
  // properly scaled. Nothing will be done for the Read More text if the
  // bottom is reached.
  self.titleLabel.font = GetFRETitleFont(self.titleLabelFontTextStyle);
  [self setReadMoreText];

  // Update the primary button once the layout changes take effect to have the
  // right measurements to evaluate the scroll position.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    [strongSelf updateViewsOnScrollViewUpdate];
    [strongSelf hideHeaderOnTallContentIfNeeded];
  });
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateViewsOnScrollViewUpdate];
}

#pragma mark - UITextViewDelegate

#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
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
#endif

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction API_AVAILABLE(ios(17.0)) {
  if (!(textView == self.disclaimerView &&
        [self.delegate respondsToSelector:@selector(didTapURLInDisclaimer:)])) {
    return defaultAction;
  }

  __weak __typeof(self) weakSelf = self;
  NSURL* URL = textItem.link;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.delegate didTapURLInDisclaimer:URL];
  }];
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

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

@end
