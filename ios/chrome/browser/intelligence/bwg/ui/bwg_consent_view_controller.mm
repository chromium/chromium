// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_view_controller.h"

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_ui_utils.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
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

// Spacing for primary and secondary buttons.
const CGFloat kSpacingPrimarySecondaryButtonsIOS26 = 4.0;
const CGFloat kSpacingPrimarySecondaryButtonsIOS18 = 0;


}  // namespace

@interface BWGConsentViewController () <UITextViewDelegate>
@end

@implementation BWGConsentViewController {
  // Main stack view. This view itself does not scroll.
  UIStackView* _mainStackView;
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

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.navigationItem.hidesBackButton = YES;
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  if (!_mainStackView) {
    [self configureMainStackView];
  }
}

#pragma mark - BWGFREViewControllerProtocol

- (CGFloat)contentHeight {
  [self.view layoutIfNeeded];
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
                     withLinkActions:@[ kBwgFootnoteLinkActionManagedAccount ]
                            inRanges:@[ [NSValue valueWithRange:linkRange] ]
                      textAttributes:textAttributes
                           fontStyle:fontStyle];
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
      @[ kBwgFirstFootnoteLinkAction, kBwgSecondFootnoteLinkAction ];
  NSArray<NSValue*>* linkRanges = @[
    [NSValue valueWithRange:link1Range], [NSValue valueWithRange:link2Range]
  ];

  return [self createAttributedString:fullText
                      withLinkActions:linkActions
                             inRanges:linkRanges
                       textAttributes:textAttributes
                            fontStyle:fontStyle];
}

// Creates an attributed string for the second box body with a link.
- (NSAttributedString*)createSecondBoxBodyAttributedText {
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
  };
  UIFontTextStyle fontStyle = UIFontTextStyleBody;

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

    NSRange linkRange = [fullText rangeOfString:linkText];

    return
        [self createAttributedString:fullText
                     withLinkActions:@[ kBwgSecondBoxLinkActionManagedAccount ]
                            inRanges:@[ [NSValue valueWithRange:linkRange] ]
                      textAttributes:textAttributes
                           fontStyle:fontStyle];
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

  NSRange link1Range = [fullText rangeOfString:link1NSString];
  NSRange link2Range = [fullText rangeOfString:link2NSString];

  NSArray<NSString*>* linkActions = @[
    kBwgSecondBoxLink1ActionNonManagedAccount,
    kBwgSecondBoxLink2ActionNonManagedAccount
  ];
  NSArray<NSValue*>* linkRanges = @[
    [NSValue valueWithRange:link1Range], [NSValue valueWithRange:link2Range]
  ];

  return [self createAttributedString:fullText
                      withLinkActions:linkActions
                             inRanges:linkRanges
                       textAttributes:textAttributes
                            fontStyle:fontStyle];
}

// Configures the main stack view and contains all the content including the
// buttons.
- (void)configureMainStackView {
  _mainStackView = [[UIStackView alloc] init];
  _mainStackView.axis = UILayoutConstraintAxisVertical;
  _mainStackView.spacing = kMainStackSpacing;

  _mainStackView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.view addSubview:_mainStackView];
  AddSameConstraintsWithInsets(
      _mainStackView, self.view,
      NSDirectionalEdgeInsetsMake(0, kMainStackHorizontalInset, 0,
                                  kMainStackHorizontalInset));
  [_mainStackView addArrangedSubview:[self createBoxesStackView]];
  [_mainStackView addArrangedSubview:[self createFootnoteView]];
  [self configureButtons];
}

// Configures primary and secondary buttons.
- (void)configureButtons {
  UIView* primaryButtonView = [self createPrimaryButton];
  [_mainStackView addArrangedSubview:primaryButtonView];
  if (@available(iOS 26, *)) {
    [_mainStackView setCustomSpacing:kSpacingPrimarySecondaryButtonsIOS26
                           afterView:primaryButtonView];
  } else {
    [_mainStackView setCustomSpacing:kSpacingPrimarySecondaryButtonsIOS18
                           afterView:primaryButtonView];
  }
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
  firstIconImageView.contentMode = UIViewContentModeScaleAspectFill;

  UIView* firstBox = [self
      createHorizontalBoxWithIcon:firstIconImageView
                          boxView:
                              [self createFirstBoxWithTitle:
                                        l10n_util::GetNSString(
                                            IDS_IOS_BWG_CONSENT_FIRST_BOX_TITLE)
                                                   bodyText:firstBody]];
  [boxesStackView addArrangedSubview:firstBox];

  NSString* secondTitle = l10n_util::GetNSString(
      _isAccountManaged ? IDS_IOS_BWG_CONSENT_MANAGED_SECOND_BOX_TITLE
                        : IDS_IOS_BWG_CONSENT_NON_MANAGED_SECOND_BOX_TITLE);

  UIImageView* secondIconImageView =
      [[UIImageView alloc] initWithImage:DefaultSymbolWithConfiguration(
                                             [self secondSymbolName], config)];

  secondIconImageView.contentMode = UIViewContentModeScaleAspectFill;

  NSAttributedString* secondBodyAttributed =
      [self createSecondBoxBodyAttributedText];
  UIView* secondBoxView = [self createSecondBoxWithTitle:secondTitle
                                      bodyAttributedText:secondBodyAttributed];

  UIView* secondBox = [self createHorizontalBoxWithIcon:secondIconImageView
                                                boxView:secondBoxView];
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
      kBwgFootNoteTextViewAccessibilityIdentifier;

  return footNoteTextView;
}

// Creates the primary button.
- (UIButton*)createPrimaryButton {
  ChromeButton* primaryButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  primaryButton.title =
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_PRIMARY_BUTTON);
  [primaryButton addTarget:self
                    action:@selector(didTapPrimaryButton:)
          forControlEvents:UIControlEventTouchUpInside];
  primaryButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_PRIMARY_BUTTON);
  primaryButton.accessibilityIdentifier =
      kBwgPrimaryButtonAccessibilityIdentifier;
  return primaryButton;
}

// Creates the secondary button.
- (UIButton*)createSecondaryButton {
  ChromeButton* secondaryButton =
      [[ChromeButton alloc] initWithStyle:ChromeButtonStyleSecondary];
  secondaryButton.title =
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_SECONDARY_BUTTON);
  [secondaryButton addTarget:self
                      action:@selector(didTapSecondaryButton:)
            forControlEvents:UIControlEventTouchUpInside];
  secondaryButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_BWG_CONSENT_SECONDARY_BUTTON);
  secondaryButton.accessibilityIdentifier =
      kBwgSecondaryButtonAccessibilityIdentifier;
  return secondaryButton;
}

// Did tap the primary button.
- (void)didTapPrimaryButton:(UIButton*)sender {
  RecordFREConsentAction(IOSGeminiFREAction::kAccept);
  [self.mutator didConsentBWG];
}

// Did tap the secondary button.
- (void)didTapSecondaryButton:(UIButton*)sender {
  RecordFREConsentAction(IOSGeminiFREAction::kDismiss);
  [self.mutator didRefuseBWGConsent];
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
          isEqualToString:kBwgFirstFootnoteLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kFirstFootnoteLinkURL)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kBwgSecondFootnoteLinkAction]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kSecondFootnoteLinkURL)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kBwgFootnoteLinkActionManagedAccount]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator openNewTabWithURL:GURL(kFootnoteLinkURLManagedAccount)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kBwgSecondBoxLinkActionManagedAccount]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator
          openNewTabWithURL:GURL(kSecondBoxLinkURLManagedAccount)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kBwgSecondBoxLink1ActionNonManagedAccount]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator
          openNewTabWithURL:GURL(kSecondBoxLink1URLNonManagedAccount)];
    }];
  }
  if ([textItem.link.absoluteString
          isEqualToString:kBwgSecondBoxLink2ActionNonManagedAccount]) {
    __weak __typeof(self) weakSelf = self;
    return [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator
          openNewTabWithURL:GURL(kSecondBoxLink2URLNonManagedAccount)];
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
