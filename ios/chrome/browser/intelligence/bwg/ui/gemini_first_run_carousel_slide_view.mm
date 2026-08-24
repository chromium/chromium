// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_carousel_slide_view.h"

#import "base/check.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {

// Normalized spacing constants.
const CGFloat kSpacingLarge = 16.0;

// Carousel slide padding.
const CGFloat kSlideTextPadding = 12.0;

// Lottie animation dimensions.
const CGFloat kLottieAnimationWidth = 327.0;
const CGFloat kLottieAnimationHeight = 180.0;
const CGFloat kLottieAnimationCornerRadius = 24.0;

// Typography.
const CGFloat kSlideTitleBaseFontSize = 25.0;
const CGFloat kSlideTitleMaxFontSize = 40.0;

}  // namespace

@implementation GeminiFirstRunCarouselSlide

- (instancetype)initWithAnimationName:(NSString*)animationName
                    darkAnimationName:(NSString*)darkAnimationName
                     animationNameRTL:(NSString*)animationNameRTL
                 darkAnimationNameRTL:(NSString*)darkAnimationNameRTL
                                title:(NSString*)title
          animationAccessibilityLabel:(NSString*)animationAccessibilityLabel {
  self = [super init];
  if (self) {
    CHECK(animationName.length);
    CHECK(darkAnimationName.length);
    CHECK(animationNameRTL.length);
    CHECK(darkAnimationNameRTL.length);
    CHECK(title.length);
    CHECK(animationAccessibilityLabel.length);
    _animationName = [animationName copy];
    _darkAnimationName = [darkAnimationName copy];
    _animationNameRTL = [animationNameRTL copy];
    _darkAnimationNameRTL = [darkAnimationNameRTL copy];
    _title = [title copy];
    _animationAccessibilityLabel = [animationAccessibilityLabel copy];
  }
  return self;
}

@end

@implementation GeminiFirstRunCarouselSlideView {
  GeminiFirstRunCarouselSlide* _slide;
  id<LottieAnimation> _lottieAnimation;
  NSString* _currentAnimationName;
  UIView* _animationContainer;
  UILabel* _titleLabel;
  UIStackView* _contentStack;
  NSLayoutConstraint* _titleWidthConstraint;
}

- (instancetype)initWithSlide:(GeminiFirstRunCarouselSlide*)slide {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    CHECK(slide);
    _slide = slide;
    self.shouldGroupAccessibilityChildren = YES;

    [self setupSubviews];
    [self setupConstraints];

    [self registerForTraitChanges:@[
      UITraitUserInterfaceStyle.class, UITraitVerticalSizeClass.class,
      UITraitPreferredContentSizeCategory.class
    ]
                       withAction:@selector(
                                      updateLayoutForCurrentTraitCollection)];
    [self updateLayoutForCurrentTraitCollection];
  }
  return self;
}

#pragma mark - Public

- (void)playAnimation {
  BOOL isCompactHeight = (self.traitCollection.verticalSizeClass ==
                          UIUserInterfaceSizeClassCompact);
  if (isCompactHeight) {
    return;
  }
  [_lottieAnimation stop];
  [_lottieAnimation play];
}

- (void)stopAnimation {
  [_lottieAnimation stop];
}

- (void)resetToFirstFrame {
  [_lottieAnimation stop];
}

#pragma mark - Private

- (BOOL)isRTL {
  return self.effectiveUserInterfaceLayoutDirection ==
         UIUserInterfaceLayoutDirectionRightToLeft;
}

- (BOOL)isDarkMode {
  return self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
}

- (NSString*)currentAnimationName {
  if ([self isRTL]) {
    return [self isDarkMode] ? _slide.darkAnimationNameRTL
                             : _slide.animationNameRTL;
  }
  return [self isDarkMode] ? _slide.darkAnimationName : _slide.animationName;
}

- (id<LottieAnimation>)createLottieAnimationWithName:(NSString*)animationName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationName;
  config.shouldLoop = NO;
  id<LottieAnimation> animation =
      ios::provider::GenerateLottieAnimation(config);
  animation.animationView.translatesAutoresizingMaskIntoConstraints = NO;
  animation.animationView.contentMode = UIViewContentModeScaleAspectFit;
  return animation;
}

- (void)setupSubviews {
  // Container view holding the active Lottie animation.
  _animationContainer = [[UIView alloc] init];
  _animationContainer.translatesAutoresizingMaskIntoConstraints = NO;
  _animationContainer.layer.cornerRadius = kLottieAnimationCornerRadius;
  _animationContainer.layer.masksToBounds = YES;
  _animationContainer.isAccessibilityElement = YES;
  _animationContainer.accessibilityTraits = UIAccessibilityTraitImage;
  _animationContainer.accessibilityLabel = _slide.animationAccessibilityLabel;

  // Configure title label.
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.textAlignment = NSTextAlignmentCenter;
  _titleLabel.numberOfLines = 0;
  _titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  [_titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  _titleLabel.accessibilityLabel = _slide.title;
  _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;

  // Vertical stack view containing the animation container and title label.
  _contentStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _animationContainer, _titleLabel ]];
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  _contentStack.axis = UILayoutConstraintAxisVertical;
  _contentStack.alignment = UIStackViewAlignmentCenter;
  _contentStack.spacing = kSpacingLarge;
  [self addSubview:_contentStack];
}

- (void)setupConstraints {
  // Pin the vertical content stack to the slide view boundaries.
  AddSameConstraints(_contentStack, self);

  // Constrain the title text width to match the animation card artwork width
  // inset by horizontal padding, preventing text from bleeding into adjacent
  // slide peeking margins.
  _titleWidthConstraint = [_titleLabel.widthAnchor
      constraintLessThanOrEqualToAnchor:_animationContainer.widthAnchor
                               constant:-2 * kSlideTextPadding];

  // Constrain the animation container to its intended card dimensions and
  // ensure the title label respects horizontal padding within the slide view.
  [NSLayoutConstraint activateConstraints:@[
    [_animationContainer.widthAnchor
        constraintLessThanOrEqualToConstant:kLottieAnimationWidth],
    [_animationContainer.heightAnchor
        constraintEqualToConstant:kLottieAnimationHeight],
    [_animationContainer.widthAnchor
        constraintLessThanOrEqualToAnchor:self.widthAnchor],

    _titleWidthConstraint,
    [_titleLabel.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor],
  ]];
}

- (UIFont*)scaledTitleFont {
  UIFont* baseFont = [UIFont systemFontOfSize:kSlideTitleBaseFontSize
                                       weight:UIFontWeightBold];
  UIFontMetrics* metrics =
      [UIFontMetrics metricsForTextStyle:UIFontTextStyleTitle2];
  return [metrics scaledFontForFont:baseFont
                   maximumPointSize:kSlideTitleMaxFontSize
      compatibleWithTraitCollection:self.traitCollection];
}

- (void)updateLayoutForCurrentTraitCollection {
  BOOL isCompactHeight = (self.traitCollection.verticalSizeClass ==
                          UIUserInterfaceSizeClassCompact);
  BOOL isLargerText = UIContentSizeCategoryCompareToCategory(
                          self.traitCollection.preferredContentSizeCategory,
                          UIContentSizeCategoryLarge) == NSOrderedDescending;

  UIFont* titleFont = [self scaledTitleFont];
  _titleLabel.font = titleFont;
  _titleLabel.attributedText =
      [GeminiUIUtils attributedStringWithGradientGeminiForTitle:_slide.title
                                                           font:titleFont];

  // In compact height, hiding the animation container automatically collapses
  // its height and the vertical stack spacing to 0.
  _animationContainer.hidden = isCompactHeight;
  if (!isCompactHeight) {
    // Swap the active Lottie animation when the interface style (light/dark) or
    // layout direction (LTR/RTL) changes.
    NSString* targetAnimationName = [self currentAnimationName];
    if (![_currentAnimationName isEqualToString:targetAnimationName]) {
      [_lottieAnimation stop];
      [_lottieAnimation.animationView removeFromSuperview];
      _lottieAnimation =
          [self createLottieAnimationWithName:targetAnimationName];
      _currentAnimationName = [targetAnimationName copy];
      if (_lottieAnimation.animationView) {
        [_animationContainer addSubview:_lottieAnimation.animationView];
        AddSameConstraints(_lottieAnimation.animationView, _animationContainer);
      }
    }
  }

  _titleWidthConstraint.active = !isLargerText;
}

@end
