// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller.h"

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Main Stack view insets and spacing.
const CGFloat kMainStackSpacing = 16.0;

// Icons attributs.
const CGFloat kIconSize = 16.0;
const CGFloat kIconImageViewTopPadding = 18.0;
const CGFloat kIconImageViewWidth = 32.0;

// Boxes stack view traits.
const CGFloat kBoxesStackViewSpacing = 2.0;
const CGFloat kBoxesStackViewCornerRadius = 16.0;

// Inner stack view spacing and padding.
const CGFloat kInnerStackViewSpacing = 6.0;
const CGFloat kInnerStackViewPadding = 12.0;

// Live Header traits.
const CGFloat kLiveHeaderIconContainerCornerRadius = 16.0;
const CGFloat kLiveHeaderIconContainerWidthMultiplier = 0.14;
const CGFloat kLiveHeaderIconPointSize = 24.0;
const CGFloat kLiveHeaderIconSizeMultiplier = 0.55;

// Symbol names not present in shared symbol_names.h.
NSString* const kWarningShieldSymbol = @"exclamationmark.shield";

// ISO alpha-2 country code for South Korea.
NSString* const kSouthKoreaCountryCode = @"kr";

}  // namespace

@interface GeminiConsentViewController () <UITextViewDelegate>
@end

@implementation GeminiConsentViewController {
  // Main stack view. This view itself does not scroll.
  UIStackView* _mainStackView;
  // Whether the account is managed.
  BOOL _isAccountManaged;
  // Type of Gemini FRE.
  GeminiFREType _FREType;
  // The country for the consent UI.
  NSString* _country;
}

- (instancetype)initWithIsAccountManaged:(BOOL)isAccountManaged
                                 FREType:(GeminiFREType)FREType
                                 country:(NSString*)country {
  self = [super init];
  if (self) {
    _isAccountManaged = isAccountManaged;
    _FREType = FREType;
    _country = country;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.navigationItem.hidesBackButton = YES;
  [self configureMainStackView];
}

#pragma mark - GeminiFREViewControllerProtocol

- (CGFloat)contentHeight {
  return
      [_mainStackView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
}

#pragma mark - Private

// Creates an attributed string with links for a given text.
- (NSAttributedString*)createAttributedString:(NSString*)text
                              withLinkActions:(NSArray<NSString*>*)linkActions
                                     inRanges:(NSArray<NSValue*>*)linkRanges
                               textAttributes:(NSDictionary*)textAttributes
                                    fontStyle:(UIFontTextStyle)fontStyle {
  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:text
                                             attributes:textAttributes];

  [linkRanges enumerateObjectsUsingBlock:^(NSValue* rangeValue, NSUInteger i,
                                           BOOL* stop) {
    NSRange range = rangeValue.rangeValue;

    NSString* linkAction = linkActions[i];

    NSDictionary* linkAttributes = @{
      NSLinkAttributeName : linkAction,
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color],
      NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
      NSFontAttributeName :
          PreferredFontForTextStyle(fontStyle, UIFontWeightSemibold)
    };

    [attributedText addAttributes:linkAttributes range:range];
  }];

  return [attributedText copy];
}

// Creates an attributed string for the footnote with hyperlinks.
- (NSAttributedString*)createFootnoteAttributedText {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = NSTextAlignmentCenter;

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSParagraphStyleAttributeName : paragraphStyle,
  };

  UIFontTextStyle fontStyle = UIFontTextStyleFootnote;

  // Consent footnote for South Korea. Managed and non-managed accounts are the
  // same.
  if ([_country isEqualToString:kSouthKoreaCountryCode]) {
    NSString* link1NSString = l10n_util::GetNSString(
        IDS_IOS_BWG_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA_LINK_1);
    NSString* link2NSString = l10n_util::GetNSString(
        IDS_IOS_BWG_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA_LINK_2);
    NSString* link3NSString = l10n_util::GetNSString(
        IDS_IOS_BWG_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA_LINK_3);

    std::vector<std::u16string> substitutions;
    substitutions.push_back(base::SysNSStringToUTF16(link1NSString));
    substitutions.push_back(base::SysNSStringToUTF16(link2NSString));
    substitutions.push_back(base::SysNSStringToUTF16(link3NSString));

    std::u16string fullTextUTF16 = base::ReplaceStringPlaceholders(
        l10n_util::GetStringUTF16(
            IDS_IOS_BWG_CONSENT_FOOTNOTE_TEXT_SOUTH_KOREA),
        substitutions, nullptr);

    NSString* fullText = base::SysUTF16ToNSString(fullTextUTF16);

    NSRange link1Range = [fullText rangeOfString:link1NSString];
    NSRange link2Range = [fullText rangeOfString:link2NSString];
    NSRange link3Range = [fullText rangeOfString:link3NSString];

    NSArray<NSString*>* linkActions = @[
      kGeminiFirstFootnoteLinkAction, kGeminiKoreanTermsLinkAction,
      kGeminiSecondFootnoteLinkAction
    ];
    NSArray<NSValue*>* linkRanges = @[
      [NSValue valueWithRange:link1Range], [NSValue valueWithRange:link2Range],
      [NSValue valueWithRange:link3Range]
    ];

    return [self createAttributedString:fullText
                        withLinkActions:linkActions
                               inRanges:linkRanges
                         textAttributes:textAttributes
                              fontStyle:fontStyle];
  } else if (_isAccountManaged) {
    // Consent footnote for managed accounts.
    NSString* linkText =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_MANAGED_LINK);
    std::u16string formatStringUTF16 =
        l10n_util::GetStringUTF16(IDS_IOS_BWG_CONSENT_FOOTNOTE_MANAGED_TEXT);

    std::vector<std::u16string> substitutions;
    substitutions.push_back(base::SysNSStringToUTF16(linkText));
    std::u16string fullTextUTF16 = base::ReplaceStringPlaceholders(
        formatStringUTF16, substitutions, nullptr);
    NSString* fullText = base::SysUTF16ToNSString(fullTextUTF16);

    NSRange linkRange = [fullText rangeOfString:linkText];

    return [self
        createAttributedString:fullText
               withLinkActions:@[ kGeminiFootnoteLinkActionManagedAccount ]
                      inRanges:@[ [NSValue valueWithRange:linkRange] ]
                textAttributes:textAttributes
                     fontStyle:fontStyle];
  } else {
    NSString* link1NSString =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_NON_MANAGED_LINK_1);
    NSString* link2NSString =
        l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_NON_MANAGED_LINK_2);

    std::vector<std::u16string> substitutions;
    substitutions.push_back(base::SysNSStringToUTF16(link1NSString));
    substitutions.push_back(base::SysNSStringToUTF16(link2NSString));

    std::u16string fullTextUTF16 = base::ReplaceStringPlaceholders(
        l10n_util::GetStringUTF16(
            IDS_IOS_BWG_CONSENT_FOOTNOTE_NON_MANAGED_TEXT),
        substitutions, nullptr);

    NSString* fullText = base::SysUTF16ToNSString(fullTextUTF16);

    NSRange link1Range = [fullText rangeOfString:link1NSString];
    NSRange link2Range = [fullText rangeOfString:link2NSString];

    NSArray<NSString*>* linkActions =
        @[ kGeminiFirstFootnoteLinkAction, kGeminiSecondFootnoteLinkAction ];
    NSArray<NSValue*>* linkRanges = @[
      [NSValue valueWithRange:link1Range], [NSValue valueWithRange:link2Range]
    ];

    return [self createAttributedString:fullText
                        withLinkActions:linkActions
                               inRanges:linkRanges
                         textAttributes:textAttributes
                              fontStyle:fontStyle];
  }
}

// Helper to construct attributed text with standard styles and specified links.
- (NSAttributedString*)createConsentBodyWithFullText:(NSString*)fullText
                                               links:(NSArray<NSString*>*)links
                                             actions:
                                                 (NSArray<NSString*>*)actions {
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
  };

  NSMutableArray<NSValue*>* linkRanges = [[NSMutableArray alloc] init];
  for (NSString* link in links) {
    NSRange range = [fullText rangeOfString:link];
    if (range.location != NSNotFound) {
      [linkRanges addObject:[NSValue valueWithRange:range]];
    }
  }

  return [self createAttributedString:fullText
                      withLinkActions:actions
                             inRanges:linkRanges
                       textAttributes:textAttributes
                            fontStyle:UIFontTextStyleBody];
}

// Creates an attributed string for the standard FRE second box body with a
// link.
- (NSAttributedString*)createStandardSecondBoxBodyAttributedText {
  if (_isAccountManaged) {
    NSString* linkText = l10n_util::GetNSString(
        IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_BODY_LINK);
    std::u16string formatStringUTF16 =
        l10n_util::GetStringUTF16(IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_BODY);

    std::vector<std::u16string> substitutions;
    substitutions.push_back(base::SysNSStringToUTF16(linkText));
    std::u16string fullTextUTF16 = base::ReplaceStringPlaceholders(
        formatStringUTF16, substitutions, nullptr);
    NSString* fullText = base::SysUTF16ToNSString(fullTextUTF16);

    return
        [self createConsentBodyWithFullText:fullText
                                      links:@[ linkText ]
                                    actions:@[
                                      kGeminiSecondBoxLinkActionManagedAccount
                                    ]];
  }

  NSString* link1NSString = l10n_util::GetNSString(
      IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY_LINK_1);
  NSString* link2NSString = l10n_util::GetNSString(
      IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY_LINK_2);

  std::vector<std::u16string> substitutions;
  substitutions.push_back(base::SysNSStringToUTF16(link1NSString));
  substitutions.push_back(base::SysNSStringToUTF16(link2NSString));

  std::u16string fullTextUTF16 = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(
          IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY),
      substitutions, nullptr);

  NSString* fullText = base::SysUTF16ToNSString(fullTextUTF16);

  return [self
      createConsentBodyWithFullText:fullText
                              links:@[ link1NSString, link2NSString ]
                            actions:@[
                              kGeminiSecondBoxLink1ActionNonManagedAccount,
                              kGeminiSecondBoxLink2ActionNonManagedAccount
                            ]];
}

// Creates an attributed string for the Live second box body with links.
- (NSAttributedString*)createLiveSecondBoxBodyAttributedText {
  // TODO(crbug.com/498291812): Replace strings placeholders.
  NSString* fullText =
      @"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      @"eiusmod tempor incididunt ut labore et dolore magna aliqua. "
      @"Gemini Apps Privacy Notice, Learn more.";

  return [self
      createConsentBodyWithFullText:fullText
                              links:@[
                                @"Gemini Apps Privacy Notice", @"Learn more"
                              ]
                            actions:@[
                              kGeminiLivePrivacyNoticeLinkAction,
                              kGeminiLiveLearnMoreLinkAction
                            ]];
}

// Creates an attributed string for the Live third box body with a link.
- (NSAttributedString*)createLiveThirdBoxBodyAttributedText {
  // TODO(crbug.com/498291812): Replace strings placeholders.
  NSString* fullText =
      @"Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
      @"Respect other's privacy ipsum dolor sit amet.";

  return [self
      createConsentBodyWithFullText:fullText
                              links:@[ @"Respect other's privacy" ]
                            actions:@[ kGeminiLivePrivacyPolicyLinkAction ]];
}

// Configures the main stack view and contains all the content including the
// buttons.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.spacing = kMainStackSpacing;

  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:_mainStackView];
  AddSameConstraints(_mainStackView, self.view);

  if (_FREType == GeminiFREType::kLive) {
    [_mainStackView addArrangedSubview:[self createLiveHeaderView]];
  }

  [_mainStackView addArrangedSubview:[self createBoxesStackView]];
  if (_FREType != GeminiFREType::kLive) {
    [_mainStackView addArrangedSubview:[self createFootnoteView]];
  }
}

// Creates the header for Live FRE with sparkle icon and title.
- (UIView*)createLiveHeaderView {
  UIView* headerView = [[UIView alloc] init];
  headerView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* verticalStack = [[UIStackView alloc] init];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.alignment = UIStackViewAlignmentCenter;
  verticalStack.spacing =
      kMainStackSpacing;  // Use main stack spacing for balance.
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;

  [headerView addSubview:verticalStack];
  AddSameConstraintsWithInsets(verticalStack, headerView,
                               NSDirectionalEdgeInsetsZero);

  UIView* iconContainer = [[UIView alloc] init];
  iconContainer.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainer.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  iconContainer.layer.cornerRadius = kLiveHeaderIconContainerCornerRadius;

  [verticalStack addArrangedSubview:iconContainer];
  [NSLayoutConstraint activateConstraints:@[
    // No hardcoded points for container size, use screen multiplier
    [iconContainer.widthAnchor
        constraintEqualToAnchor:headerView.widthAnchor
                     multiplier:kLiveHeaderIconContainerWidthMultiplier],
    [iconContainer.heightAnchor
        constraintEqualToAnchor:iconContainer.widthAnchor]
  ]];

  UIImageView* iconView = [[UIImageView alloc] init];
  // Relate Point size to screen scale where possible
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kLiveHeaderIconPointSize
                          weight:UIImageSymbolWeightMedium];
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  iconView.image = CustomSymbolWithConfiguration(kGeminiLiveLogoSymbol, config);
#else
  iconView.image =
      CustomSymbolWithConfiguration(kGeminiNonBrandedLogoSymbol, config);
#endif
  iconView.contentMode = UIViewContentModeScaleAspectFit;
  iconView.translatesAutoresizingMaskIntoConstraints = NO;

  [iconContainer addSubview:iconView];
  [NSLayoutConstraint activateConstraints:@[
    [iconView.centerXAnchor
        constraintEqualToAnchor:iconContainer.centerXAnchor],
    [iconView.centerYAnchor
        constraintEqualToAnchor:iconContainer.centerYAnchor],
    // Proportionate to container
    [iconView.widthAnchor
        constraintEqualToAnchor:iconContainer.widthAnchor
                     multiplier:kLiveHeaderIconSizeMultiplier],
    [iconView.heightAnchor
        constraintEqualToAnchor:iconContainer.heightAnchor
                     multiplier:kLiveHeaderIconSizeMultiplier]
  ]];

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.numberOfLines = 0;
  titleLabel.textAlignment = NSTextAlignmentCenter;
  NSMutableAttributedString* attributedTitle =
      [[NSMutableAttributedString alloc]
          initWithString:@"Go Live with Gemini"
              attributes:@{
                NSFontAttributeName : PreferredFontForTextStyle(
                    UIFontTextStyleTitle2, UIFontWeightRegular),
                NSForegroundColorAttributeName :
                    [UIColor colorNamed:kTextPrimaryColor]
              }];
  NSRange geminiRange = [attributedTitle.string rangeOfString:@"Gemini"];
  if (geminiRange.location != NSNotFound) {
    [attributedTitle addAttribute:NSForegroundColorAttributeName
                            value:[UIColor colorNamed:kBlue600Color]
                            range:geminiRange];
  }
  titleLabel.attributedText = attributedTitle;
  [verticalStack addArrangedSubview:titleLabel];

  return headerView;
}

// Creates the standard FRE horizontal boxes stack view.
- (NSArray<UIView*>*)createStandardBoxesWithConfig:
    (UIImageSymbolConfiguration*)config {
  NSMutableArray<UIView*>* boxes = [[NSMutableArray alloc] init];

  // First Box
  UIImage* icon1 = CustomSymbolWithConfiguration(kPhoneSparkleSymbol, config);
  NSString* title1 =
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FIRST_BOX_TITLE);
  NSString* bodyText1 = l10n_util::GetNSString(
      _isAccountManaged ? IDS_IOS_BWG_CONSENT_MANAGED_FIRST_BOX_BODY
                        : IDS_IOS_BWG_CONSENT_NON_MANAGED_FIRST_BOX_BODY);
  UIView* innerBox1 = [self createFirstBoxWithTitle:title1 bodyText:bodyText1];
  UIImageView* boxIcon1 = [[UIImageView alloc] initWithImage:icon1];
  boxIcon1.contentMode = UIViewContentModeScaleAspectFit;
  [boxes addObject:[self createHorizontalBoxWithIcon:boxIcon1
                                             boxView:innerBox1]];

  // Second Box
  UIImage* icon2 =
      DefaultSymbolWithConfiguration([self secondSymbolName], config);
  NSString* title2 = l10n_util::GetNSString(
      _isAccountManaged ? IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_TITLE
                        : IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_TITLE);
  NSAttributedString* bodyText2 =
      [self createStandardSecondBoxBodyAttributedText];
  UIView* innerBox2 = [self createSecondBoxWithTitle:title2
                                  bodyAttributedText:bodyText2];
  UIImageView* boxIcon2 = [[UIImageView alloc] initWithImage:icon2];
  boxIcon2.contentMode = UIViewContentModeScaleAspectFit;
  [boxes addObject:[self createHorizontalBoxWithIcon:boxIcon2
                                             boxView:innerBox2]];

  return boxes;
}

// Creates the Live FRE horizontal boxes stack view.
- (NSArray<UIView*>*)createLiveBoxesWithConfig:
    (UIImageSymbolConfiguration*)config {
  NSMutableArray<UIView*>* boxes = [[NSMutableArray alloc] init];

  // First Box
  UIImage* icon1 = DefaultSymbolWithConfiguration(kMicrophoneSymbol, config);
  // TODO(crbug.com/498291812): Replace strings placeholders.
  NSString* bodyText1 =
      @"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do "
      @"eiusmod tempor incididunt ut labore et dolore magna aliqua.";
  UIView* innerBox1 = [self createFirstBoxWithTitle:nil bodyText:bodyText1];
  UIImageView* boxIcon1 = [[UIImageView alloc] initWithImage:icon1];
  boxIcon1.contentMode = UIViewContentModeScaleAspectFit;
  [boxes addObject:[self createHorizontalBoxWithIcon:boxIcon1
                                             boxView:innerBox1]];

  // Second Box
  UIImage* icon2 = DefaultSymbolWithConfiguration(kInfoCircleSymbol, config);
  NSAttributedString* bodyText2 = [self createLiveSecondBoxBodyAttributedText];
  UIView* innerBox2 = [self createSecondBoxWithTitle:nil
                                  bodyAttributedText:bodyText2];
  UIImageView* boxIcon2 = [[UIImageView alloc] initWithImage:icon2];
  boxIcon2.contentMode = UIViewContentModeScaleAspectFit;
  [boxes addObject:[self createHorizontalBoxWithIcon:boxIcon2
                                             boxView:innerBox2]];

  // Third Box
  UIImage* icon3 = DefaultSymbolWithConfiguration(kWarningShieldSymbol, config);
  NSAttributedString* bodyText3 = [self createLiveThirdBoxBodyAttributedText];
  UIView* innerBox3 = [self createSecondBoxWithTitle:nil
                                  bodyAttributedText:bodyText3];
  UIImageView* boxIcon3 = [[UIImageView alloc] initWithImage:icon3];
  boxIcon3.contentMode = UIViewContentModeScaleAspectFit;
  [boxes addObject:[self createHorizontalBoxWithIcon:boxIcon3
                                             boxView:innerBox3]];

  return boxes;
}

// Creates the horizontal boxes stack view.
- (UIStackView*)createBoxesStackView {
  UIStackView* boxesStackView = [[UIStackView alloc] init];
  boxesStackView.axis = UILayoutConstraintAxisVertical;
  boxesStackView.spacing = kBoxesStackViewSpacing;
  boxesStackView.layer.cornerRadius = kBoxesStackViewCornerRadius;
  boxesStackView.clipsToBounds = YES;
  boxesStackView.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kIconSize
                          weight:UIImageSymbolWeightMedium];

  NSArray<UIView*>* boxes = (_FREType == GeminiFREType::kLive)
                                ? [self createLiveBoxesWithConfig:config]
                                : [self createStandardBoxesWithConfig:config];

  for (UIView* box in boxes) {
    [boxesStackView addArrangedSubview:box];
  }

  return boxesStackView;
}

// Creates horizontal stack view with icon and box view.
- (UIView*)createHorizontalBoxWithIcon:(UIImageView*)iconImageView
                               boxView:(UIView*)boxView {
  UIStackView* horizontalStackView = [[UIStackView alloc] init];
  horizontalStackView.alignment = UIStackViewAlignmentTop;
  horizontalStackView.translatesAutoresizingMaskIntoConstraints = NO;
  horizontalStackView.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];

  iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSquareConstraints(iconImageView, kIconSize);

  UIView* iconContainerView = [[UIView alloc] init];
  iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [iconContainerView addSubview:iconImageView];
  [horizontalStackView addArrangedSubview:iconContainerView];

  [NSLayoutConstraint activateConstraints:@[
    [iconImageView.centerXAnchor
        constraintEqualToAnchor:iconContainerView.centerXAnchor],
    [iconImageView.topAnchor constraintEqualToAnchor:iconContainerView.topAnchor
                                            constant:kIconImageViewTopPadding],
    [iconContainerView.widthAnchor
        constraintEqualToAnchor:iconImageView.widthAnchor
                       constant:kIconImageViewWidth],
  ]];

  [horizontalStackView addArrangedSubview:boxView];

  return horizontalStackView;
}

// Gets the second SF Symbol name.
- (NSString*)secondSymbolName {
  if (_isAccountManaged) {
    return kBuilding2Symbol;
  }
  if (@available(iOS 18, *)) {
    return kCounterClockWiseSymbol;
  }
  return kHistorySymbol;
}

// Creates the first box view containing the text and the title.
- (UIView*)createFirstBoxWithTitle:(NSString*)titleText
                          bodyText:(NSString*)bodyText {
  UIView* boxView = [[UIView alloc] init];
  boxView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* innerStackView = [[UIStackView alloc] init];
  innerStackView.axis = UILayoutConstraintAxisVertical;
  innerStackView.alignment = UIStackViewAlignmentFill;
  innerStackView.spacing = kInnerStackViewSpacing;

  innerStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [boxView addSubview:innerStackView];

  AddSameConstraintsWithInsets(
      innerStackView, boxView,
      NSDirectionalEdgeInsetsMake(kInnerStackViewPadding, 0,
                                  kInnerStackViewPadding,
                                  kInnerStackViewPadding));

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = titleText;
  titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);
  titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;

  titleLabel.numberOfLines = 0;
  [innerStackView addArrangedSubview:titleLabel];

  UILabel* bodyLabel = [[UILabel alloc] init];
  bodyLabel.text = bodyText;
  bodyLabel.font = PreferredFontForTextStyle(UIFontTextStyleBody);
  bodyLabel.numberOfLines = 0;
  bodyLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [innerStackView addArrangedSubview:bodyLabel];

  return boxView;
}

// Creates the second box view containing the title and an attributed body text.
- (UIView*)createSecondBoxWithTitle:(NSString*)titleText
                 bodyAttributedText:(NSAttributedString*)bodyAttributedText {
  UIView* boxView = [[UIView alloc] init];
  boxView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* innerStackView = [[UIStackView alloc] init];
  innerStackView.axis = UILayoutConstraintAxisVertical;
  innerStackView.alignment = UIStackViewAlignmentFill;
  innerStackView.spacing = kInnerStackViewSpacing;

  innerStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [boxView addSubview:innerStackView];

  AddSameConstraintsWithInsets(
      innerStackView, boxView,
      NSDirectionalEdgeInsetsMake(kInnerStackViewPadding, 0,
                                  kInnerStackViewPadding,
                                  kInnerStackViewPadding));

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = titleText;
  titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);
  titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;

  titleLabel.numberOfLines = 0;
  [innerStackView addArrangedSubview:titleLabel];

  UITextView* bodyTextView = [[UITextView alloc] init];
  bodyTextView.backgroundColor = [UIColor clearColor];
  bodyTextView.scrollEnabled = NO;
  bodyTextView.editable = NO;
  bodyTextView.textDragInteraction.enabled = NO;
  bodyTextView.delegate = self;
  bodyTextView.textContainerInset = UIEdgeInsetsZero;
  bodyTextView.textContainer.lineFragmentPadding = 0;
  bodyTextView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color]};
  bodyTextView.attributedText = bodyAttributedText;
  [innerStackView addArrangedSubview:bodyTextView];

  return boxView;
}

// Creates the foot note view.
- (UITextView*)createFootnoteView {
  UITextView* footNoteTextView = [[UITextView alloc] init];
  footNoteTextView.backgroundColor = [UIColor clearColor];
  footNoteTextView.scrollEnabled = NO;
  footNoteTextView.editable = NO;
  footNoteTextView.textDragInteraction.enabled = NO;
  footNoteTextView.delegate = self;
  footNoteTextView.textContainerInset = UIEdgeInsetsZero;
  footNoteTextView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color]};
  footNoteTextView.attributedText = [self createFootnoteAttributedText];
  footNoteTextView.accessibilityIdentifier =
      kGeminiFootNoteTextViewAccessibilityIdentifier;

  return footNoteTextView;
}

#pragma mark - UITextViewDelegate

// Handles tap on UITextView.
- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  if (!textItem.link) {
    return nil;
  }

  RecordFREConsentAction(IOSGeminiFREAction::kLinkClick);
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiFirstFootnoteLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kFirstFootnoteLinkURL)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiSecondFootnoteLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kSecondFootnoteLinkURL)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiFootnoteLinkActionManagedAccount]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kFootnoteLinkURLManagedAccount)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiSecondBoxLinkActionManagedAccount]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator
          openNewTabWithURL:GURL(kSecondBoxLinkURLManagedAccount)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiSecondBoxLink1ActionNonManagedAccount]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator
          openNewTabWithURL:GURL(kSecondBoxLink1URLNonManagedAccount)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiSecondBoxLink2ActionNonManagedAccount]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator
          openNewTabWithURL:GURL(kSecondBoxLink2URLNonManagedAccount)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiLivePrivacyNoticeLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kLivePrivacyNoticeLinkURL)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiLiveLearnMoreLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kLiveLearnMoreLinkURL)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiLivePrivacyPolicyLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kLivePrivacyPolicyLinkURL)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kGeminiKoreanTermsLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kKoreanTermsFootnoteLinkURL)];
    }];
  }
  return defaultAction;
}

// If the text item is a link, return nil to prevent the long-press context menu
// from appearing.
- (UIMenu*)textView:(UITextView*)textView
    menuConfigurationForTextItem:(UITextItem*)textItem
                     defaultMenu:(UIMenu*)defaultMenu {
  if (textItem.link) {
    return nil;
  }
  return defaultMenu;
}

@end
