// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_interstitial/ui_bundled/incognito_interstitial_view_controller.h"

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/incognito_interstitial/ui_bundled/incognito_interstitial_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/util/attributed_string_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Opacity of navigation bar should be 0% at offset keyframe 0 and 100% at
// keyframe 1.
const CGFloat kNavigationBarFadeInKeyFrame0 = 70;
const CGFloat kNavigationBarFadeInKeyFrame1 = 140;
const CGFloat kNavigationBarFadeInCompactHeightKeyFrame0 = 20;
const CGFloat kNavigationBarFadeInCompactHeightKeyFrame1 = 50;

// Name of banner at the top of the view.
NSString* const kIncognitoInterstitialBannerName =
    @"incognito_interstitial_screen_banner";

// Maximum number of lines for the URL label, before the user unfolds it.
const int kURLLabelDefaultNumberOfLines = 3;

// Line height multiple for the title label.
const CGFloat kTitleLabelLineHeightMultiple = 1.3;

}  // namespace

@interface IncognitoInterstitialViewController ()

// The navigation bar to display at the top of the view, to contain a "Cancel"
// button.
@property(nonatomic, strong) UINavigationBar* navigationBar;

// Vertical offset of internal scroll view, to update navigation bar opacity.
@property(nonatomic, assign) CGFloat scrollViewContentOffsetY;

// Label to display the URL which is going to be opened.
@property(nonatomic, strong) UILabel* URLLabel;

// Button which allows the user to remove the limit on the number of lines
// for `URLLabel`.
@property(nonatomic, strong) UIButton* expandURLButton;

// Whether the number of lines of `URLLabel` is unlimited.
@property(nonatomic, assign) BOOL URLIsExpanded;

@end

@implementation IncognitoInterstitialViewController

@dynamic delegate;

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      kIncognitoInterstitialAccessibilityIdentifier;

  self.bannerName = kIncognitoInterstitialBannerName;
  self.bannerSize = BannerImageSizeType::kStandard;
  self.shouldBannerFillTopSpace = YES;
  self.shouldHideBanner = IsCompactHeight(self.traitCollection);

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_INTERSTITIAL_TITLE);
  self.title = title;
  self.titleText = title;
  self.titleHorizontalMargin = 0;
  self.primaryActionString = l10n_util::GetNSString(
      IDS_IOS_INCOGNITO_INTERSTITIAL_OPEN_IN_CHROME_INCOGNITO);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_INTERSTITIAL_OPEN_IN_CHROME);

  // This needs to be called after parameters of `PromoStyleViewController` have
  // been set, but before adding additional layout constraints, since these
  // constraints can only be activated once the complete view hierarchy has been
  // constructed and relevant views belong to the same hierarchy.
  [super viewDidLoad];

  // Fix the line height multiple of `self.titleLabel`.
  [self fixTitleLabelLineHeightMultiple];

  self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  self.modalInPresentation = YES;

  // Creating the Incognito view (same one as NTP).
  IncognitoView* incognitoView =
      [[IncognitoView alloc] initWithFrame:CGRectZero
             showTopIncognitoImageAndTitle:NO
                 stackViewHorizontalMargin:0
                         stackViewMaxWidth:CGFLOAT_MAX];
  incognitoView.URLLoaderDelegate = self.URLLoaderDelegate;
  incognitoView.translatesAutoresizingMaskIntoConstraints = NO;
  incognitoView.bounces = NO;

  // The Incognito view is put inside a container because it might try
  // to put constraints on its superview.
  UIView* incognitoViewContainer = [[UIView alloc] init];
  incognitoViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [incognitoViewContainer addSubview:incognitoView];

  // A stack view is created to contain the URL label as well as the
  // Incognito view container.
  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.URLLabel, incognitoViewContainer ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisVertical;
  [self.specificContentView addSubview:stackView];

  // Create the gradient view located on the leading end of the "more" button
  // which lets the user unfold the URL label.
  UIView* gradientView = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, self.URLLabel.font.lineHeight,
                               self.URLLabel.font.lineHeight)];
  gradientView.translatesAutoresizingMaskIntoConstraints = NO;
  gradientView.backgroundColor = self.view.backgroundColor;
  CAGradientLayer* gradientLayer = [CAGradientLayer layer];
  gradientLayer.frame = gradientView.bounds;
  gradientLayer.startPoint = CGPointMake(0.0, 0.0);
  gradientLayer.endPoint = CGPointMake(1.0, 0.0);
  gradientLayer.colors =
      @[ (id)[UIColor clearColor].CGColor, (id)[UIColor whiteColor].CGColor ];
  gradientView.layer.mask = gradientLayer;
  [self.expandURLButton addSubview:gradientView];
  // Add the "more" button to the content view.
  [self.specificContentView addSubview:self.expandURLButton];

  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self.delegate
                           action:@selector(didTapCancelButton)];
  cancelButton.accessibilityIdentifier =
      kIncognitoInterstitialCancelButtonAccessibilityIdentifier;

  UINavigationItem* navigationRootItem =
      [[UINavigationItem alloc] initWithTitle:@""];
  navigationRootItem.rightBarButtonItem = cancelButton;

  self.navigationBar = [[UINavigationBar alloc] init];
  [self.navigationBar pushNavigationItem:navigationRootItem animated:false];
  [self updateNavigationBarAppearance];

  [NSLayoutConstraint activateConstraints:@[
    [stackView.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [stackView.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
    [stackView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [stackView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [incognitoView.heightAnchor
        constraintEqualToAnchor:incognitoView.contentLayoutGuide.heightAnchor],
    [incognitoViewContainer.leadingAnchor
        constraintEqualToAnchor:incognitoView.leadingAnchor],
    [incognitoViewContainer.trailingAnchor
        constraintEqualToAnchor:incognitoView.trailingAnchor],
    [incognitoViewContainer.topAnchor
        constraintEqualToAnchor:incognitoView.topAnchor],
    [incognitoViewContainer.bottomAnchor
        constraintEqualToAnchor:incognitoView.bottomAnchor],
    [gradientView.trailingAnchor
        constraintEqualToAnchor:self.expandURLButton.leadingAnchor],
    [gradientView.bottomAnchor
        constraintEqualToAnchor:self.expandURLButton.bottomAnchor],
    [gradientView.topAnchor
        constraintEqualToAnchor:self.expandURLButton.topAnchor],
    [gradientView.widthAnchor
        constraintEqualToAnchor:gradientView.heightAnchor],
    [self.expandURLButton.trailingAnchor
        constraintEqualToAnchor:self.URLLabel.trailingAnchor],
    [self.expandURLButton.bottomAnchor
        constraintEqualToAnchor:self.URLLabel.bottomAnchor],
  ]];

  [self.view addSubview:self.navigationBar];
  self.navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.navigationBar.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.navigationBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.navigationBar.topAnchor constraintEqualToAnchor:self.view.topAnchor],
  ]];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitVerticalSizeClass.self ]);
    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      weakSelf.shouldHideBanner = IsCompactHeight(traitEnvironment);
      [weakSelf updateNavigationBarAppearance];
    };
    [self registerForTraitChanges:traits withHandler:handler];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  self.shouldHideBanner = IsCompactHeight(self.traitCollection);
  [self updateNavigationBarAppearance];
}
#endif

- (void)viewDidLayoutSubviews {
  if (self.URLIsExpanded) {
    self.expandURLButton.hidden = YES;
    self.URLLabel.numberOfLines = 0;
  } else {
    CGRect URLLabelBoundingRect = [self.URLLabel.text
        boundingRectWithSize:CGSizeMake(self.URLLabel.bounds.size.width,
                                        CGFLOAT_MAX)
                     options:NSStringDrawingUsesLineFragmentOrigin
                  attributes:@{NSFontAttributeName : self.URLLabel.font}
                     context:nil];
    int expandedNumberOfLines =
        URLLabelBoundingRect.size.height / self.URLLabel.font.lineHeight;
    self.expandURLButton.hidden =
        (expandedNumberOfLines <= kURLLabelDefaultNumberOfLines);
  }
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // Ensure the title label is focused when the Incognito interstial appears.
  UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                  self.titleLabel);
}

#pragma mark - Accessors

- (UILabel*)URLLabel {
  if (!_URLLabel) {
    _URLLabel = [[UILabel alloc] initWithFrame:self.view.frame];
    _URLLabel.lineBreakMode = NSLineBreakByClipping;
    _URLLabel.text = self.URLText;
    _URLLabel.numberOfLines = kURLLabelDefaultNumberOfLines;
    _URLLabel.textAlignment = NSTextAlignmentCenter;
    _URLLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _URLLabel.accessibilityIdentifier =
        kIncognitoInterstitialURLLabelAccessibilityIdentifier;
  }
  return _URLLabel;
}

- (UIButton*)expandURLButton {
  if (!_expandURLButton) {
    __weak __typeof(self) weakSelf = self;
    UIAction* readMoreAction = [UIAction actionWithHandler:^(id sender) {
      [weakSelf expandURLButtonWasTapped];
    }];

    NSAttributedString* readMoreString = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(
                           IDS_IOS_INCOGNITO_INTERSTITIAL_URL_READ_MORE_BUTTON)
            attributes:@{
              NSFontAttributeName : _URLLabel.font,
              NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]
            }];

    _expandURLButton =
        [[ExtendedTouchTargetButton alloc] initWithFrame:CGRectZero
                                           primaryAction:readMoreAction];

    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(0, 0, 0, 0);
    buttonConfiguration.attributedTitle = readMoreString;
    _expandURLButton.configuration = buttonConfiguration;

    _expandURLButton.backgroundColor = self.view.backgroundColor;
    _expandURLButton.translatesAutoresizingMaskIntoConstraints = NO;
    // On voice over, the full info is on the URL field and this button isn't
    // needed.
    _expandURLButton.accessibilityElementsHidden = YES;
  }
  return _expandURLButton;
}

#pragma mark - UIScrollViewDelegate

// This override allows scroll detection of the scroll view contained within the
// underlying PromoStyleViewController.
- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // Constrain vertical content offset to positive values only.
  CGPoint contentOffset = scrollView.contentOffset;
  contentOffset.y = fmax(0, contentOffset.y);
  scrollView.contentOffset = contentOffset;

  self.scrollViewContentOffsetY = scrollView.contentOffset.y;
  [self updateNavigationBarAppearance];

  [super scrollViewDidScroll:scrollView];
}

#pragma mark - Private

- (void)updateNavigationBarAppearance {
  CGFloat keyFrame0 = IsCompactHeight(self.traitCollection)
                          ? kNavigationBarFadeInCompactHeightKeyFrame0
                          : kNavigationBarFadeInKeyFrame0;
  CGFloat keyFrame1 = IsCompactHeight(self.traitCollection)
                          ? kNavigationBarFadeInCompactHeightKeyFrame1
                          : kNavigationBarFadeInKeyFrame1;
  CGFloat opacity =
      (self.scrollViewContentOffsetY - keyFrame0) / (keyFrame1 - keyFrame0);
  opacity = std::clamp(opacity, 0.0, 1.0, std::less_equal<>());

  UIColor* backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  UIColor* shadowColor = [UIColor colorNamed:kSeparatorColor];
  CGFloat backgroundAlpha = CGColorGetAlpha(backgroundColor.CGColor);
  CGFloat shadowAlpha = CGColorGetAlpha(shadowColor.CGColor);

  UINavigationBarAppearance* appearance =
      [[UINavigationBarAppearance alloc] init];
  [appearance configureWithOpaqueBackground];
  appearance.backgroundColor =
      [backgroundColor colorWithAlphaComponent:backgroundAlpha * opacity];
  appearance.shadowColor =
      [shadowColor colorWithAlphaComponent:shadowAlpha * opacity];

  self.navigationBar.compactAppearance = appearance;
  self.navigationBar.standardAppearance = appearance;
  self.navigationBar.scrollEdgeAppearance = appearance;

  self.navigationBar.tintColor = [UIColor colorNamed:kBlueColor];
}

// Called when `expandURLButton` is tapped.
- (void)expandURLButtonWasTapped {
  self.URLIsExpanded = YES;
  [self.view setNeedsLayout];
}

// Set the `attributedText` attribute of `self.titleLabel` to customize the line
// height multiple.
- (void)fixTitleLabelLineHeightMultiple {
  NSMutableAttributedString* titleAttributedText =
      [NSAttributedStringFromUILabel(self.titleLabel) mutableCopy];
  NSMutableDictionary* attributes = [NSMutableDictionary
      dictionaryWithDictionary:[titleAttributedText attributesAtIndex:0
                                                       effectiveRange:nil]];
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  [paragraphStyle setParagraphStyle:attributes[NSParagraphStyleAttributeName]];
  paragraphStyle.lineHeightMultiple = kTitleLabelLineHeightMultiple;
  attributes[NSParagraphStyleAttributeName] = paragraphStyle;
  [titleAttributedText
      setAttributes:attributes
              range:NSMakeRange(0, titleAttributedText.length)];
  self.titleLabel.attributedText = titleAttributedText;
}

@end
