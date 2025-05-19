// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kLensUserEducationLightMode = @"lens_usered_lightmode";

NSString* const kLensUserEducationDarkMode = @"lens_usered_darkmode";

NSString* const kLensOverlayOnboardingImageName =
    @"lens_overlay_onboarding_illustration";

// The height of the animation, as a percentage of the whole view minus the
// fixed height items. By subtracting out the height of the items with a
// fixed height, and sizing the animationa based on what is left we can
// scale more accurately on larger and smaller screens.

// Pause button right padding.
const CGFloat kPauseButtonRightPadding = 12;
// Pause button bottom padding.
const CGFloat kPauseButtonBottomPadding = 14;
// The size of the onboarding illustration.
const CGFloat kLensOverlayOnboardingIllustrationSize = 80;
// The size of the onboarding symbols.
const CGFloat kLensOverlayOnboaridingSymbolSize = 22;
// The value that makes the Lottie animation loop indefinitely.
const CGFloat kLottieInfiniteLoopFlag = -1;
// The height of the invariant items of the dialog
// (e.g. bottom action buttons, the padding).
const CGFloat kDialogFixedItemsHeight = 160;
// The width of the dialog in regular display size.
const CGFloat kDialogWidthInRegularDisplaySize = 540;

// Whether to use the updated onboarding string.
bool UseUpdatedStrings() {
  auto treatment = GetLensOverlayOnboardingTreatment();
  return treatment ==
             LensOverlayOnboardingTreatment::kUpdatedOnboardingStrings ||
         treatment == LensOverlayOnboardingTreatment::
                          kUpdatedOnboardingStringsAndVisuals;
}

// Whether to use the updated onboarding graphics.
bool UseUpdatedGraphics() {
  return GetLensOverlayOnboardingTreatment() ==
         LensOverlayOnboardingTreatment::kUpdatedOnboardingStringsAndVisuals;
}

NSString* TitleString() {
  if (UseUpdatedStrings()) {
    return l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_ONBOARDING_TITLE);
  } else {
    return l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_CONSENT_TITLE);
  }
}

NSString* DescriptionString() {
  if (UseUpdatedStrings()) {
    return l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_ONBOARDING_DESCRIPTION);
  } else {
    return l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_CONSENT_DESCRIPTION);
  }
}

NSString* PrimaryActionString() {
  if (UseUpdatedStrings()) {
    return l10n_util::GetNSString(
        IDS_IOS_LENS_OVERLAY_ONBOARDING_BUTTON_SEARCH);
  } else {
    return l10n_util::GetNSString(
        IDS_IOS_LENS_OVERLAY_CONSENT_ACCEPT_TERMS_BUTTON_TITLE);
  }
}

NSString* SecondaryActionString() {
  if (UseUpdatedStrings()) {
    return l10n_util::GetNSString(
        IDS_IOS_LENS_OVERLAY_ONBOARDING_BUTTON_CANCEL);
  } else {
    return l10n_util::GetNSString(
        IDS_IOS_LENS_OVERLAY_CONSENT_DENY_TERMS_BUTTON_TITLE);
  }
}

NSString* LearnMoreString() {
  if (UseUpdatedStrings()) {
    return l10n_util::GetNSString(
        IDS_IOS_LENS_OVERLAY_ONBOARDING_LEARN_MORE_ACTION);
  } else {
    return l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_CONSENT_LEARN_MORE);
  }
}

}  // namespace

@interface LensOverlayConsentViewController () <UITextViewDelegate>
@end

@implementation LensOverlayConsentViewController {
  id<LottieAnimation> _animationViewWrapper;
  BOOL _isAnimationPlaying;
  UIButton* _animationPlayerButton;
  UIStackView* _contentStack;
}

// Property tagged dynamic because it overrides super class delegate with and
// extension of the super delegate type  (i.e.
// LensOverlayConsentViewControllerDelegate extends
// PromoStyleViewControllerDelegate).
@dynamic delegate;

- (void)viewDidLoad {
  self.layoutBehindNavigationBar = YES;
  self.shouldHideBanner = YES;
  self.headerImageType = PromoStyleImageType::kNone;

  // Avoid extra top spacing.
  self.subtitleBottomMargin = 0;
  self.headerImageBottomMargin = 0;

  _contentStack = [self createContentStack];
  [self.specificContentView addSubview:_contentStack];

  self.primaryActionString = PrimaryActionString();
  self.secondaryActionString = SecondaryActionString();

  [super viewDidLoad];
  [NSLayoutConstraint activateConstraints:@[
    [self.specificContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_contentStack.heightAnchor],
  ]];
  AddSameConstraintsToSides(
      _contentStack, self.specificContentView,
      LayoutSides::kTrailing | LayoutSides::kLeading | LayoutSides::kTop);
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_animationViewWrapper play];
  _isAnimationPlaying = YES;
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self.view);
}

- (CGSize)preferredContentSize {
  [_contentStack layoutIfNeeded];
  CGFloat presentedContentHeight = _contentStack.frame.size.height;

  // Only regular width is relevant, as the bottom sheet prresentation is
  // edge-attached in compact width.
  return CGSizeMake(kDialogWidthInRegularDisplaySize,
                    presentedContentHeight + kDialogFixedItemsHeight);
}

#pragma mark - Private

- (UIView*)createAnimationView {
  // Lottie animation.
  _animationViewWrapper =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
          ? [self createAnimation:kLensUserEducationDarkMode]
          : [self createAnimation:kLensUserEducationLightMode];

  UIView* animationView = _animationViewWrapper.animationView;

  animationView.translatesAutoresizingMaskIntoConstraints = NO;
  animationView.contentMode = UIViewContentModeScaleAspectFit;

  _animationPlayerButton = [self newAnimationPlayerButton];

  [_animationPlayerButton addTarget:self
                             action:@selector(animationPlayerButtonTapped)
                   forControlEvents:UIControlEventTouchUpInside];

  [animationView addSubview:_animationPlayerButton];
  [NSLayoutConstraint activateConstraints:@[
    [_animationPlayerButton.rightAnchor
        constraintEqualToAnchor:animationView.rightAnchor
                       constant:-kPauseButtonRightPadding],
    [_animationPlayerButton.bottomAnchor
        constraintEqualToAnchor:animationView.bottomAnchor
                       constant:-kPauseButtonBottomPadding]
  ]];

  return animationView;
}

- (UIImageView*)createOnboardingImageView {
  UIImageView* imageView = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:kLensOverlayOnboardingImageName]];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSizeConstraints(imageView,
                     CGSizeMake(kLensOverlayOnboardingIllustrationSize,
                                kLensOverlayOnboardingIllustrationSize));

  return imageView;
}

- (UIStackView*)createContentStack {
  // Title/description labels.
  UILabel* titleLabel = [self createLabel:TitleString()
                                     font:GetFRETitleFont(UIFontTextStyleTitle2)
                                    color:kTextPrimaryColor];

  NSString* description = DescriptionString();
  NSString* learnMore = LearnMoreString();

  UIView* bodyText;
  if (UseUpdatedStrings()) {
    NSString* descriptionWithAction =
        [NSString stringWithFormat:@"%@ %@", description, learnMore];
    NSMutableAttributedString* attributedText =
        [[NSMutableAttributedString alloc]
            initWithString:descriptionWithAction
                attributes:[self descriptionTextAttributes]];
    // The URL in the text attribute is empty as the delegate is responsible for
    // opening external links.
    NSRange urlRange = NSMakeRange(description.length + 1, learnMore.length);
    [attributedText addAttribute:NSLinkAttributeName
                           value:[[NSURL alloc] init]
                           range:urlRange];
    bodyText = [self createTextViewWithAttributedString:attributedText];
  } else {
    bodyText =
        [self createLabel:DescriptionString()
                     font:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]
                    color:kLensOverlayConsentDialogDescriptionColor];
  }

  // Clear `titleText` and `subtitleText` so that PromoStyleViewController does
  // not use them to create alternate title and subtitle labels.
  self.titleText = nil;
  self.subtitleText = nil;

  UIStackView* stack;
  if (UseUpdatedGraphics()) {
    UIImageView* imageView = [self createOnboardingImageView];
    stack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ imageView, titleLabel, bodyText ]];
  } else {
    UIView* animationView = [self createAnimationView];
    stack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ animationView, titleLabel, bodyText ]];
  }

  if (!UseUpdatedStrings()) {
    __weak __typeof(self) weakSelf = self;
    UIButton* learnMoreLink =
        [self plainButtonWithTitle:learnMore
                     actionHandler:^(UIAction* action) {
                       [weakSelf.delegate didPressLearnMore];
                     }];

    [stack addArrangedSubview:learnMoreLink];
  }

  stack.axis = UILayoutConstraintAxisVertical;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.alignment = UIStackViewAlignmentCenter;
  stack.spacing = 20;
  [stack setCustomSpacing:8 afterView:titleLabel];
  [stack setCustomSpacing:8 afterView:bodyText];

  return stack;
}

#pragma mark - UITextViewDelegate

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction API_AVAILABLE(ios(17.0)) {
  if (textItem.contentType == UITextItemContentTypeLink) {
    [self.delegate didPressLearnMore];
  }

  return nil;
}

#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.delegate didPressLearnMore];
  // Prevent the system from executing the default URL open action.
  return NO;
}
#endif

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Make the textView not selectable while allowing interactions with the
  // embedded links.
  textView.selectedTextRange = nil;
}

#pragma mark - private

- (NSDictionary*)descriptionTextAttributes {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  return textAttributes;
}

- (UITextView*)createTextViewWithAttributedString:
    (NSAttributedString*)attributedText {
  UITextView* textView = [[UITextView alloc] init];
  textView.delegate = self;
  textView.attributedText = attributedText;
  textView.editable = NO;
  textView.adjustsFontForContentSizeCategory = YES;
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  textView.scrollEnabled = NO;

  return textView;
}

// Creates a label with the given  string, font, and color.
- (UILabel*)createLabel:(NSString*)text
                   font:(UIFont*)font
                  color:(NSString*)colorName {
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.text = text;
  label.numberOfLines = 0;
  label.font = font;
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:colorName];
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

- (UIButton*)plainButtonWithTitle:(NSString*)title
                    actionHandler:(UIActionHandler)handler {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.title = title;

  UIButton* button =
      [UIButton buttonWithConfiguration:buttonConfiguration
                          primaryAction:[UIAction actionWithHandler:handler]];
  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.pointerInteractionEnabled = YES;
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  button.titleLabel.textAlignment = NSTextAlignmentCenter;

  return button;
}

- (UIButton*)newAnimationPlayerButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateDefaultEffectCirclePointerStyleProvider();
  button.tintColor =
      [UIColor colorNamed:kLensOverlayConsentDialogAnimationPlayerButtonColor];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kLensOverlayOnboaridingSymbolSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  [button setPreferredSymbolConfiguration:symbolConfig
                          forImageInState:UIControlStateNormal];
  [button setImage:DefaultSymbolWithPointSize(kPauseButton,
                                              kLensOverlayOnboaridingSymbolSize)
          forState:UIControlStateNormal];
  button.imageView.contentMode = UIViewContentModeScaleAspectFit;

  return button;
}

- (void)animationPlayerButtonTapped {
  _isAnimationPlaying = !_isAnimationPlaying;

  if (_isAnimationPlaying) {
    [_animationPlayerButton
        setImage:DefaultSymbolWithPointSize(kPauseButton,
                                            kLensOverlayOnboaridingSymbolSize)
        forState:UIControlStateNormal];
    [_animationViewWrapper play];
  } else {
    [_animationPlayerButton
        setImage:DefaultSymbolWithPointSize(kPlayButton,
                                            kLensOverlayOnboaridingSymbolSize)
        forState:UIControlStateNormal];
    [_animationViewWrapper pause];
  }
}

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = kLottieInfiniteLoopFlag;
  return ios::provider::GenerateLottieAnimation(config);
}

@end
