// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_view_controller.h"

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_ui_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Main Stack view insets and spacing.
const CGFloat kMainStackHorizontalInset = 20.0;
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

// Action identifier on a tap on links in the footnote.
NSString* const kFirstFootnoteLinkAction = @"firstFootnoteLinkAction";
NSString* const kSecondFootnoteLinkAction = @"secondFootnoteLinkAction";
NSString* const kFootnoteLinkActionManagedAccount =
    @"footnoteLinkActionManagedAccount";

// Links for attributed links.
const char kFirstFootnoteLinkURL[] = "https://policies.google.com/terms";
const char kSecondFootnoteLinkURL[] =
    "https://support.google.com/gemini/answer/13594961";
const char kFootnoteLinkURLManagedAccount[] =
    "https://support.google.com/a/answer/15706919";

}  // namespace

@interface BWGConsentViewController () <UITextViewDelegate>
@end

@implementation BWGConsentViewController {
  // The root vertical stack view that arranges the UI sections of the
  // screen. It holds the `_contentScrollView` and the
  // fixed action buttons at the bottom. This view itself does not scroll.
  UIStackView* _mainStackView;
  // A scroll view that contains the `_contentStackView`. This allows the main
  // content (info boxes, footnote) to scroll vertically if it
  // doesn't fit on the screen.
  UIScrollView* _contentScrollView;
  // The vertical stack view placed inside the `_contentScrollView`. It arranges
  // the actual informational UI elements, such as the info boxes and the
  // footnote, which are intended to be scrolled together.
  UIStackView* _contentStackView;
  // Whether the account is managed.
  BOOL _isAccountManaged;
}

- (instancetype)initWithIsAccountManaged:(BOOL)isAccountManaged {
  self = [super init];
  if (self) {
    _isAccountManaged = isAccountManaged;
  }
  return self;
}

#pragma mark - UIViewController

// TODO(crbug.com/414777915): Implement a basic UI.
- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.navigationItem.hidesBackButton = YES;
  [self setupStackViews];
}

#pragma mark - Public

- (CGFloat)contentHeight {
  return
      [_mainStackView systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height +
      [_contentStackView
          systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
          .height;
}

#pragma mark - Private

// Configures all the stacks.
- (void)setupStackViews {
  [self configureMainStackView];
  [self configureContentViews];
  [_mainStackView addArrangedSubview:_contentScrollView];
  [self configureButtons];
}

// Configures the scrollable content area, including the scroll view and its
// content stack view.
- (void)configureContentViews {
  _contentScrollView = [[UIScrollView alloc] init];
  _contentScrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _contentScrollView.showsVerticalScrollIndicator = NO;

  _contentStackView = [[UIStackView alloc] init];
  _contentStackView.axis = UILayoutConstraintAxisVertical;
  _contentStackView.spacing = kMainStackSpacing;
  _contentStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [_contentScrollView addSubview:_contentStackView];

  AddSameConstraints(_contentStackView, _contentScrollView);

  [NSLayoutConstraint activateConstraints:@[
    [_contentStackView.widthAnchor
        constraintEqualToAnchor:_contentScrollView.widthAnchor]
  ]];

  [_contentStackView addArrangedSubview:[self createBoxesStackView]];
  [_contentStackView addArrangedSubview:[self createFootnoteView]];
}

// Creates an attributed string for the footnote with hyperlinks.
- (NSAttributedString*)createFootnoteAttributedText {
  if (_isAccountManaged) {
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

    return
        [self createAttributedString:fullText
                     withLinkActions:@[ kFootnoteLinkActionManagedAccount ]
                            inRanges:@[ [NSValue valueWithRange:linkRange] ]];
  }
  NSString* link1NSString =
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_NON_MANAGED_LINK_1);
  NSString* link2NSString =
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_FOOTNOTE_NON_MANAGED_LINK_2);

  std::vector<std::u16string> substitutions;
  substitutions.push_back(base::SysNSStringToUTF16(link1NSString));
  substitutions.push_back(base::SysNSStringToUTF16(link2NSString));

  std::u16string fullTextUTF16 = base::ReplaceStringPlaceholders(
      l10n_util::GetStringUTF16(IDS_IOS_BWG_CONSENT_FOOTNOTE_NON_MANAGED_TEXT),
      substitutions, nullptr);

  NSString* fullText = base::SysUTF16ToNSString(fullTextUTF16);

  NSRange link1Range = [fullText rangeOfString:link1NSString];
  NSRange link2Range = [fullText rangeOfString:link2NSString];

  NSArray<NSString*>* linkActions =
      @[ kFirstFootnoteLinkAction, kSecondFootnoteLinkAction ];
  NSArray<NSValue*>* linkRanges = @[
    [NSValue valueWithRange:link1Range], [NSValue valueWithRange:link2Range]
  ];

  return [self createAttributedString:fullText
                      withLinkActions:linkActions
                             inRanges:linkRanges];
}

- (NSAttributedString*)createAttributedString:(NSString*)text
                              withLinkActions:(NSArray<NSString*>*)linkActions
                                     inRanges:(NSArray<NSValue*>*)linkRanges {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = NSTextAlignmentCenter;

  NSDictionary* baseTextAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSParagraphStyleAttributeName : paragraphStyle,
  };

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:text
                                             attributes:baseTextAttributes];

  [linkRanges enumerateObjectsUsingBlock:^(NSValue* rangeValue, NSUInteger i,
                                           BOOL* stop) {
    NSRange range = rangeValue.rangeValue;

    NSString* linkAction = linkActions[i];

    NSDictionary* linkAttributes = @{
      NSLinkAttributeName : linkAction,
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
      NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
      NSFontAttributeName : PreferredFontForTextStyle(UIFontTextStyleFootnote,
                                                      UIFontWeightSemibold)
    };

    [attributedText addAttributes:linkAttributes range:range];
  }];

  return [attributedText copy];
}

// Configures the main stack view.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.spacing = kMainStackSpacing;

  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:_mainStackView];
  AddSameConstraintsWithInsets(
      _mainStackView, self.view.safeAreaLayoutGuide,
      NSDirectionalEdgeInsetsMake(0, kMainStackHorizontalInset, 0,
                                  kMainStackHorizontalInset));
}

// Configures primary and secondary buttons.
- (void)configureButtons {
  UIView* primaryButtonView = [self createPrimaryButton];
  [_mainStackView addArrangedSubview:primaryButtonView];
  [_mainStackView setCustomSpacing:0.0 afterView:primaryButtonView];
  [_mainStackView addArrangedSubview:[self createSecondaryButton]];
}

// Creates the 2 horizontal boxes stack view.
- (UIStackView*)createBoxesStackView {
  UIStackView* boxesStackView = [[UIStackView alloc] init];
  boxesStackView.axis = UILayoutConstraintAxisVertical;
  boxesStackView.spacing = kBoxesStackViewSpacing;
  boxesStackView.layer.cornerRadius = kBoxesStackViewCornerRadius;
  boxesStackView.clipsToBounds = YES;
  boxesStackView.translatesAutoresizingMaskIntoConstraints = NO;

  NSString* firstBody = l10n_util::GetNSString(
      _isAccountManaged ? IDS_IOS_BWG_CONSENT_MANAGED_FIRST_BOX_BODY
                        : IDS_IOS_BWG_CONSENT_NON_MANAGED_FIRST_BOX_BODY);

  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithPointSize:kIconSize
                          weight:UIImageSymbolWeightMedium];

  UIImageView* firstIconImageView = [[UIImageView alloc]
      initWithImage:CustomSymbolWithConfiguration(kPhoneSparkleSymbol, config)];
  firstIconImageView.contentMode = UIViewContentModeScaleAspectFit;

  UIView* firstBox = [self
      createHorizontalBoxWithIcon:firstIconImageView
                          boxView:
                              [self createBoxWithTitle:
                                        l10n_util::GetNSString(
                                            IDS_IOS_BWG_CONSENT_FIRST_BOX_TITLE)
                                              bodyText:firstBody]];
  [boxesStackView addArrangedSubview:firstBox];

  NSString* secondTitle = l10n_util::GetNSString(
      _isAccountManaged ? IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_TITLE
                        : IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_TITLE);

  NSString* secondBody = l10n_util::GetNSString(
      _isAccountManaged ? IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_BODY
                        : IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_BODY);

  UIImageView* secondIconImageView =
      _isAccountManaged
          ? [[UIImageView alloc] initWithImage:DefaultSymbolWithConfiguration(
                                                   kBuilding2Symbol, config)]
          : [[UIImageView alloc]
                initWithImage:DefaultSymbolWithConfiguration(
                                  kCounterClockWiseSymbol, config)];

  secondIconImageView.contentMode = UIViewContentModeScaleAspectFit;

  UIView* secondBox =
      [self createHorizontalBoxWithIcon:secondIconImageView
                                boxView:[self createBoxWithTitle:secondTitle
                                                        bodyText:secondBody]];
  [boxesStackView addArrangedSubview:secondBox];
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
  [NSLayoutConstraint activateConstraints:@[
    [iconImageView.widthAnchor constraintEqualToConstant:kIconSize],
    [iconImageView.heightAnchor constraintEqualToConstant:kIconSize]
  ]];

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

// Creates the bow view containing the text and the title.
- (UIView*)createBoxWithTitle:(NSString*)titleText
                     bodyText:(NSString*)bodyText {
  UIView* boxView = [[UIView alloc] init];
  boxView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* innerStackView = [[UIStackView alloc] init];
  innerStackView.axis = UILayoutConstraintAxisVertical;
  innerStackView.alignment = UIStackViewAlignmentLeading;
  innerStackView.spacing = kInnerStackViewSpacing;

  innerStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [boxView addSubview:innerStackView];

  CGFloat innerPadding = kInnerStackViewPadding;
  AddSameConstraintsWithInsets(
      innerStackView, boxView,
      NSDirectionalEdgeInsetsMake(innerPadding, 0, innerPadding, innerPadding));

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = titleText;
  titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);

  titleLabel.numberOfLines = 0;
  [innerStackView addArrangedSubview:titleLabel];

  UILabel* bodyLabel = [[UILabel alloc] init];
  bodyLabel.text = bodyText;
  bodyLabel.font = PreferredFontForTextStyle(UIFontTextStyleBody);
  bodyLabel.numberOfLines = 0;
  bodyLabel.textColor = [UIColor colorNamed:kGrey700Color];
  [innerStackView addArrangedSubview:bodyLabel];

  return boxView;
}

// Creates the foot note view.
- (UITextView*)createFootnoteView {
  UITextView* footNoteTextView = [[UITextView alloc] init];
  footNoteTextView.backgroundColor = [UIColor clearColor];
  footNoteTextView.scrollEnabled = NO;
  footNoteTextView.editable = NO;
  footNoteTextView.delegate = self;

  footNoteTextView.textContainerInset = UIEdgeInsetsZero;
  footNoteTextView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  footNoteTextView.attributedText = [self createFootnoteAttributedText];

  return footNoteTextView;
}

// Creates the primary button.
- (UIButton*)createPrimaryButton {
  UIButton* primaryButton = [BWGUIUtils
      createPrimaryButtonWithTitle:l10n_util::GetNSString(
                                       IDS_IOS_BWG_CONSENT_PRIMARY_BUTTON)];
  [primaryButton addTarget:self
                    action:@selector(didTapPrimaryButton:)
          forControlEvents:UIControlEventTouchUpInside];
  primaryButton.accessibilityLabel = @"Consent Primary Action";
  return primaryButton;
}

// Creates the secondary button.
- (UIButton*)createSecondaryButton {
  UIButton* secondaryButton = [BWGUIUtils
      createSecondaryButtonWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_BWG_CONSENT_SECONDARY_BUTTON)];
  [secondaryButton addTarget:self
                      action:@selector(didTapSecondaryButton:)
            forControlEvents:UIControlEventTouchUpInside];
  // TODO(crbug.com/420643840): Add a11y labels.
  return secondaryButton;
}

// Did tap the primary button.
- (void)didTapPrimaryButton:(UIButton*)sender {
  [self.mutator didConsentBWG];
}

// Did tap the secondary button.
- (void)didTapSecondaryButton:(UIButton*)sender {
  [self.mutator didRefuseBWGConsent];
}

#pragma mark - UITextViewDelegate

// Handles tap on UITextView.
- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  if (!textItem.link) {
    return defaultAction;
  }
  if ([textItem.link.absoluteString isEqualToString:kFirstFootnoteLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kFirstFootnoteLinkURL)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kSecondFootnoteLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kSecondFootnoteLinkURL)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kFootnoteLinkActionManagedAccount]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kFootnoteLinkURLManagedAccount)];
    }];
  }
  return defaultAction;
}

@end
