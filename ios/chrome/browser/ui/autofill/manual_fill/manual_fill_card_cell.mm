// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_card_cell.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/payments/payments_service_url.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credit_card.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_labeled_chip.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using autofill::CreditCard::RecordType::kVirtualCard;
using base::SysNSStringToUTF8;

@interface ManualFillCardItem ()

// The content delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentInjector>
    contentInjector;

// The navigation delegate for this item.
@property(nonatomic, weak, readonly) id<CardListDelegate> navigationDelegate;

// The credit card for this item.
@property(nonatomic, readonly) ManualFillCreditCard* card;

@end

@implementation ManualFillCardItem

- (instancetype)initWithCreditCard:(ManualFillCreditCard*)card
                   contentInjector:
                       (id<ManualFillContentInjector>)contentInjector
                navigationDelegate:(id<CardListDelegate>)navigationDelegate {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _contentInjector = contentInjector;
    _navigationDelegate = navigationDelegate;
    _card = card;
    self.cellClass = [ManualFillCardCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillCardCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithCreditCard:self.card
            contentInjector:self.contentInjector
         navigationDelegate:self.navigationDelegate];
}

@end

@interface ManualFillCardCell () <UITextViewDelegate>

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// The label with bank name and network.
@property(nonatomic, strong) UILabel* cardLabel;

// The credit card icon.
@property(nonatomic, strong) UIImageView* cardIcon;

// The text view with instructions for how to use virtual cards.
@property(nonatomic, strong) UITextView* virtualCardInstructionTextView;

// A labeled chip showing the card number.
@property(nonatomic, strong) ManualFillLabeledChip* cardNumberLabeledChip;

// A button showing the card number.
@property(nonatomic, strong) UIButton* cardNumberButton;

// A labeled chip showing the cardholder name.
@property(nonatomic, strong) ManualFillLabeledChip* cardholderLabeledChip;

// A button showing the cardholder name.
@property(nonatomic, strong) UIButton* cardholderButton;

// A labeled chip showing the card's expiration date.
@property(nonatomic, strong) ManualFillLabeledChip* expirationDateLabeledChip;

// A button showing the expiration month.
@property(nonatomic, strong) UIButton* expirationMonthButton;

// A button showing the expiration year.
@property(nonatomic, strong) UIButton* expirationYearButton;

// The content delegate for this item.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

// The navigation delegate for this item.
@property(nonatomic, weak) id<CardListDelegate> navigationDelegate;

// The credit card data for this cell.
@property(nonatomic, weak) ManualFillCreditCard* card;

// Layout guide for the cell's content.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

@end

@implementation ManualFillCardCell

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];

  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

  self.cardLabel.text = @"";

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVirtualCards)) {
    self.virtualCardInstructionTextView.text = @"";
    self.virtualCardInstructionTextView.hidden = NO;

    [self.cardNumberLabeledChip prepareForReuse];
    [self.expirationDateLabeledChip prepareForReuse];
    [self.cardholderLabeledChip prepareForReuse];
  } else {
    // TODO(crbug.com/330329960): Deprecate button use once
    // kAutofillEnableVirtualCards is enabled.
    [self.cardNumberButton setTitle:@"" forState:UIControlStateNormal];
    [self.cardholderButton setTitle:@"" forState:UIControlStateNormal];
    [self.expirationMonthButton setTitle:@"" forState:UIControlStateNormal];
    [self.expirationYearButton setTitle:@"" forState:UIControlStateNormal];

    self.cardNumberButton.hidden = NO;
    self.cardholderButton.hidden = NO;
  }

  self.contentInjector = nil;
  self.navigationDelegate = nil;
  self.cardIcon.image = nil;
  self.card = nil;
}

- (void)setUpWithCreditCard:(ManualFillCreditCard*)card
            contentInjector:(id<ManualFillContentInjector>)contentInjector
         navigationDelegate:(id<CardListDelegate>)navigationDelegate {
  if (!self.dynamicConstraints) {
    self.dynamicConstraints = [[NSMutableArray alloc] init];
  }

  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }

  self.contentInjector = contentInjector;
  self.navigationDelegate = navigationDelegate;
  self.card = card;

  [self populateViewsWithCardData:card];
  [self verticallyArrangeViews:card];
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.layoutGuide = AddLayoutGuideToContentView(self.contentView);

  self.selectionStyle = UITableViewCellSelectionStyleNone;

  // Create the UIViews, add them to the contentView.
  self.cardLabel = CreateLabel();
  [self.contentView addSubview:self.cardLabel];
  self.cardIcon = [[UIImageView alloc] init];
  self.cardIcon.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.cardIcon];
  UILabel* expirationDateSeparatorLabel;

  // If Virtual Cards are enabled, create UIViews with the labeled chips,
  // otherwise use the buttons.
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVirtualCards)) {
    // Virtual card instruction textview is always created, but hidden for
    // non-virtual cards.
    self.virtualCardInstructionTextView =
        [self createVirtualCardInstructionTextView];
    [self.contentView addSubview:self.virtualCardInstructionTextView];
    self.cardNumberLabeledChip = [[ManualFillLabeledChip alloc]
        initSingleChipWithTarget:self
                        selector:@selector(userDidTapCardNumber:)];
    [self.contentView addSubview:self.cardNumberLabeledChip];

    self.expirationDateLabeledChip = [[ManualFillLabeledChip alloc]
        initExpirationDateChipWithTarget:self
                           monthSelector:@selector(userDidTapExpirationMonth:)
                            yearSelector:@selector(userDidTapExpirationYear:)];
    [self.contentView addSubview:self.expirationDateLabeledChip];
    self.cardholderLabeledChip = [[ManualFillLabeledChip alloc]
        initSingleChipWithTarget:self
                        selector:@selector(userDidTapCardholderName:)];
    [self.contentView addSubview:self.cardholderLabeledChip];
  } else {
    // TODO(crbug.com/330329960): Deprecate button use once
    // kAutofillEnableVirtualCards is enabled.
    self.cardNumberButton =
        CreateChipWithSelectorAndTarget(@selector(userDidTapCardNumber:), self);
    [self.contentView addSubview:self.cardNumberButton];
    self.expirationMonthButton =
        CreateChipWithSelectorAndTarget(@selector(userDidTapCardInfo:), self);
    [self.contentView addSubview:self.expirationMonthButton];
    expirationDateSeparatorLabel = [self createExpirationSeparatorLabel];
    [self.contentView addSubview:expirationDateSeparatorLabel];
    self.expirationYearButton =
        CreateChipWithSelectorAndTarget(@selector(userDidTapCardInfo:), self);
    [self.contentView addSubview:self.expirationYearButton];
    self.cardholderButton =
        CreateChipWithSelectorAndTarget(@selector(userDidTapCardInfo:), self);
    [self.contentView addSubview:self.cardholderButton];
  }

  [self horizontallyArrangeViews:expirationDateSeparatorLabel];
}

// Horizontally positions the UIViews.
- (void)horizontallyArrangeViews:(UILabel*)expirationDateSeparatorLabel {
  CreateGraySeparatorForContainer(self.contentView);

  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.cardIcon, self.cardLabel ], self.layoutGuide);
  [NSLayoutConstraint activateConstraints:@[
    [self.cardIcon.centerYAnchor
        constraintEqualToAnchor:self.cardLabel.centerYAnchor]
  ]];

  // If Virtual Cards are enabled, position the labeled chips, else position the
  // regular buttons.
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVirtualCards)) {
      AppendHorizontalConstraintsForViews(
          staticConstraints, @[ self.virtualCardInstructionTextView ],
          self.layoutGuide);
    AppendHorizontalConstraintsForViews(
        staticConstraints, @[ self.cardNumberLabeledChip ], self.layoutGuide,
        kChipsHorizontalMargin,
        AppendConstraintsHorizontalEqualOrSmallerThanGuide);
    AppendHorizontalConstraintsForViews(
        staticConstraints, @[ self.expirationDateLabeledChip ],
        self.layoutGuide, kChipsHorizontalMargin,
        AppendConstraintsHorizontalEqualOrSmallerThanGuide);
    AppendHorizontalConstraintsForViews(
        staticConstraints, @[ self.cardholderLabeledChip ], self.layoutGuide,
        kChipsHorizontalMargin,
        AppendConstraintsHorizontalEqualOrSmallerThanGuide);
  } else {
    // TODO(crbug.com/330329960): Deprecate button use once
    // kAutofillEnableVirtualCards is enabled.
    AppendHorizontalConstraintsForViews(
        staticConstraints, @[ self.cardNumberButton ], self.layoutGuide,
        kChipsHorizontalMargin,
        AppendConstraintsHorizontalEqualOrSmallerThanGuide);
    AppendHorizontalConstraintsForViews(
        staticConstraints,
        @[
          self.expirationMonthButton, expirationDateSeparatorLabel,
          self.expirationYearButton
        ],
        self.layoutGuide, kChipsHorizontalMargin,
        AppendConstraintsHorizontalSyncBaselines |
            AppendConstraintsHorizontalEqualOrSmallerThanGuide);
    AppendHorizontalConstraintsForViews(
        staticConstraints, @[ self.cardholderButton ], self.layoutGuide,
        kChipsHorizontalMargin,
        AppendConstraintsHorizontalEqualOrSmallerThanGuide);
  }

  // Without this set, Voice Over will read the content vertically instead of
  // horizontally.
  self.contentView.shouldGroupAccessibilityChildren = YES;

  [NSLayoutConstraint activateConstraints:staticConstraints];
}

// Adds the data from the ManualFillCreditCard to the corresponding UIViews.
- (void)populateViewsWithCardData:(ManualFillCreditCard*)card {
  self.cardIcon.image = NativeImage(card.issuerNetworkIconID);

  // If Virtual Cards are enabled set text for labeled chips, else set text for
  // buttons.
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVirtualCards)) {
    NSMutableAttributedString* attributedString =
        [self createCardLabelAttributedText:card];
    self.cardLabel.numberOfLines = 0;
    self.cardLabel.attributedText = attributedString;
    self.cardLabel.accessibilityIdentifier = attributedString.string;
    if (card.recordType == kVirtualCard) {
      self.virtualCardInstructionTextView.attributedText =
          [self createvirtualCardInstructionTextViewAttributedText];
      self.virtualCardInstructionTextView.backgroundColor = UIColor.clearColor;
    }
    [self.cardNumberLabeledChip
        setLabelText:
            (card.recordType == kVirtualCard
                 ? l10n_util::GetNSString(
                       IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CARD_NUMBER_LABEL_IOS)
                 : l10n_util::GetNSString(
                       IDS_AUTOFILL_REGULAR_CARD_MANUAL_FALLBACK_BUBBLE_CARD_NUMBER_LABEL_IOS))
        buttonTitles:@[ card.obfuscatedNumber ]];
    [self.expirationDateLabeledChip
        setLabelText:
            l10n_util::GetNSString(
                IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_EXP_DATE_LABEL_IOS)
        buttonTitles:@[ card.expirationMonth, card.expirationYear ]];
    [self.cardholderLabeledChip
        setLabelText:
            l10n_util::GetNSString(
                IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_NAME_ON_CARD_LABEL_IOS)
        buttonTitles:@[ card.cardHolder ]];
  } else {
    // TODO(crbug.com/330329960): Deprecate button use once
    // kAutofillEnableVirtualCards is enabled.
    NSString* cardName = [self createCardName:card];
    self.cardLabel.attributedText = [[NSMutableAttributedString alloc]
        initWithString:cardName
            attributes:@{
              NSForegroundColorAttributeName :
                  [UIColor colorNamed:kTextPrimaryColor],
              NSFontAttributeName :
                  [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
            }];

    [self.cardNumberButton setTitle:card.obfuscatedNumber
                           forState:UIControlStateNormal];
    [self.expirationMonthButton setTitle:card.expirationMonth
                                forState:UIControlStateNormal];
    [self.expirationYearButton setTitle:card.expirationYear
                               forState:UIControlStateNormal];
    [self.cardholderButton setTitle:card.cardHolder
                           forState:UIControlStateNormal];
  }
}

// Positions the UIViews vertically.
- (void)verticallyArrangeViews:(ManualFillCreditCard*)card {
  // Holds the views whose leading anchor is constrained relative to the cell's
  // leading anchor.
  std::vector<ManualFillCellView> verticalLeadViews;
  AddViewToVerticalLeadViews(self.cardLabel,
                             ManualFillCellView::ElementType::kOther,
                             verticalLeadViews);

  // Holds the chip buttons related to the card that are vertical leads.
  NSMutableArray<UIView*>* cardInfoGroupVerticalLeadChips =
      [[NSMutableArray alloc] init];

  // If Virtual Cards are enabled add labeled chips to be positioned
  // else just add the buttons.
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVirtualCards)) {
    // Virtual card instruction.
    if (card.recordType == kVirtualCard) {
      AddViewToVerticalLeadViews(self.virtualCardInstructionTextView,
                                 ManualFillCellView::ElementType::kOther,
                                 verticalLeadViews);
      self.virtualCardInstructionTextView.hidden = NO;
    } else {
      self.virtualCardInstructionTextView.hidden = YES;
    }

    // Card number labeled chip button.
    [self addChipButton:self.cardNumberLabeledChip
            toChipGroup:cardInfoGroupVerticalLeadChips
                 ifTrue:(card.obfuscatedNumber.length > 0)];

    // Expiration date labeled chip button.
    [cardInfoGroupVerticalLeadChips addObject:self.expirationDateLabeledChip];

    // Card holder labeled chip button.
    [self addChipButton:self.cardholderLabeledChip
            toChipGroup:cardInfoGroupVerticalLeadChips
                 ifTrue:(card.cardHolder.length > 0)];
  } else {
    // TODO(crbug.com/330329960): Deprecate button use once
    // kAutofillEnableVirtualCards is enabled.

    // Card number chip button.
    [self addChipButton:self.cardNumberButton
            toChipGroup:cardInfoGroupVerticalLeadChips
                 ifTrue:(card.obfuscatedNumber.length > 0)];

    // Expiration date chip button.
    [cardInfoGroupVerticalLeadChips addObject:self.expirationMonthButton];

    // Card holder chip button.
    [self addChipButton:self.cardholderButton
            toChipGroup:cardInfoGroupVerticalLeadChips
                 ifTrue:(card.cardHolder.length > 0)];
  }

  AddChipGroupsToVerticalLeadViews(@[ cardInfoGroupVerticalLeadChips ],
                                   verticalLeadViews);

  // Set and activate constraints.
  AppendVerticalConstraintsSpacingForViews(self.dynamicConstraints,
                                           verticalLeadViews, self.layoutGuide);
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

- (void)userDidTapCardNumber:(UIButton*)sender {
  NSString* number = self.card.number;
  if (![self.contentInjector canUserInjectInPasswordField:NO
                                            requiresHTTPS:YES]) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVirtualCards)) {
    base::RecordAction(base::UserMetricsAction(
        [self createMetricsAction:@"SelectCardNumber"]));
  } else {
    base::RecordAction(
        base::UserMetricsAction("ManualFallback_CreditCard_SelectCardNumber"));
  }

  if (self.card.canFillDirectly) {
    [self.contentInjector userDidPickContent:number
                               passwordField:NO
                               requiresHTTPS:YES];
  } else {
    [self.navigationDelegate
        requestFullCreditCard:self.card
                    fieldType:manual_fill::PaymentFieldType::kCardNumber];
  }
}

// TODO(crbug.com/330329960): Deprecate this method once
// kAutofillEnableVirtualCards is enabled.
- (void)userDidTapCardInfo:(UIButton*)sender {
  const char* metricsAction = nullptr;
  if (sender == self.cardholderButton) {
    metricsAction = "ManualFallback_CreditCard_SelectCardholderName";
  } else if (sender == self.expirationMonthButton) {
    metricsAction = "ManualFallback_CreditCard_SelectExpirationMonth";
  } else if (sender == self.expirationYearButton) {
    metricsAction = "ManualFallback_CreditCard_SelectExpirationYear";
  }
  DCHECK(metricsAction);
  base::RecordAction(base::UserMetricsAction(metricsAction));

  [self.contentInjector userDidPickContent:sender.titleLabel.text
                             passwordField:NO
                             requiresHTTPS:NO];
}

- (void)userDidTapCardholderName:(UIButton*)sender {
  base::RecordAction(base::UserMetricsAction(
      [self createMetricsAction:@"SelectCardholderName"]));
  [self.contentInjector userDidPickContent:sender.titleLabel.text
                             passwordField:NO
                             requiresHTTPS:NO];
}

- (void)userDidTapExpirationMonth:(UIButton*)sender {
  base::RecordAction(base::UserMetricsAction(
      [self createMetricsAction:@"SelectExpirationMonth"]));
  if (self.card.recordType == kVirtualCard) {
    [self.navigationDelegate
        requestFullCreditCard:self.card
                    fieldType:manual_fill::PaymentFieldType::kExpirationMonth];
  } else {
    [self.contentInjector userDidPickContent:sender.titleLabel.text
                               passwordField:NO
                               requiresHTTPS:NO];
  }
}

- (void)userDidTapExpirationYear:(UIButton*)sender {
  base::RecordAction(base::UserMetricsAction(
      [self createMetricsAction:@"SelectExpirationYear"]));
  if (self.card.recordType == kVirtualCard) {
    [self.navigationDelegate
        requestFullCreditCard:self.card
                    fieldType:manual_fill::PaymentFieldType::kExpirationYear];
  } else {
    [self.contentInjector userDidPickContent:sender.titleLabel.text
                               passwordField:NO
                               requiresHTTPS:NO];
  }
}

- (const char*)createMetricsAction:(NSString*)selectedChip {
  return [NSString stringWithFormat:@"ManualFallback_%@_%@",
                                    self.card.recordType == kVirtualCard
                                        ? @"VirtualCard"
                                        : @"CreditCard",
                                    selectedChip]
      .UTF8String;
}

- (NSString*)createCardName:(ManualFillCreditCard*)card {
  NSString* cardName;
  // TODO: b/322543459 Take out deprecated bank name, add functionality for card
  // product name.
  if (card.bankName.length) {
    cardName = card.network;
  } else {
    cardName =
        [NSString stringWithFormat:@"%@ %@", card.network, card.bankName];
  }
  return cardName;
}

// Creates the attributed string containing the card name and potentially a
// virtual card subtitle for the card label.
- (NSMutableAttributedString*)createCardLabelAttributedText:
    (ManualFillCreditCard*)card {
  NSString* cardName = [self createCardName:card];
  NSString* virtualCardSubtitle =
      card.recordType == kVirtualCard
          ? l10n_util::GetNSString(
                IDS_AUTOFILL_VIRTUAL_CARD_SUGGESTION_OPTION_VALUE)
          : nil;

  return CreateHeaderAttributedString(cardName, virtualCardSubtitle);
}

// Creates the attributed string for virtual card instructions.
- (NSMutableAttributedString*)
    createvirtualCardInstructionTextViewAttributedText {
  NSMutableAttributedString* virtualCardInstructionAttributedString =
      [[NSMutableAttributedString alloc]
          initWithString:
              [NSString
                  stringWithFormat:
                      @"%@ ",
                      l10n_util::GetNSString(
                          IDS_AUTOFILL_PAYMENTS_MANUAL_FALLBACK_VIRTUAL_CARD_INSTRUCTION_TEXT)]
              attributes:@{
                NSForegroundColorAttributeName :
                    [UIColor colorNamed:kTextSecondaryColor],
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
              }];

  NSString* learnMoreString = l10n_util::GetNSString(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK_LABEL);
  NSMutableAttributedString* virtualCardLearnMoreAttributedString =
      [[NSMutableAttributedString alloc]
          initWithString:learnMoreString
              attributes:@{
                NSForegroundColorAttributeName :
                    [UIColor colorNamed:kBlueColor],
                NSFontAttributeName :
                    [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
              }];
  [virtualCardLearnMoreAttributedString
      addAttribute:NSLinkAttributeName
             value:@"unused"
             range:[learnMoreString rangeOfString:learnMoreString]];

  [virtualCardInstructionAttributedString
      appendAttributedString:virtualCardLearnMoreAttributedString];
  return virtualCardInstructionAttributedString;
}

- (UITextView*)createVirtualCardInstructionTextView {
  UITextView* virtualCardInstructionTextView =
      [[UITextView alloc] initWithFrame:self.contentView.frame];
  virtualCardInstructionTextView.scrollEnabled = NO;
  virtualCardInstructionTextView.editable = NO;
  virtualCardInstructionTextView.delegate = self;
  virtualCardInstructionTextView.translatesAutoresizingMaskIntoConstraints = NO;
  virtualCardInstructionTextView.textColor =
      [UIColor colorNamed:kTextSecondaryColor];
  virtualCardInstructionTextView.backgroundColor = UIColor.clearColor;
  virtualCardInstructionTextView.textContainerInset =
      UIEdgeInsetsMake(0, 0, 0, 0);
  virtualCardInstructionTextView.textContainer.lineFragmentPadding = 0;
  return virtualCardInstructionTextView;
}

// TODO(crbug.com/330329960): Deprecate this method use once
// kAutofillEnableVirtualCards is enabled.
- (UILabel*)createExpirationSeparatorLabel {
  UILabel* expirationSeparatorLabel = CreateLabel();
  expirationSeparatorLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  [expirationSeparatorLabel setTextColor:[UIColor colorNamed:kSeparatorColor]];
  expirationSeparatorLabel.text = @"/";
  return expirationSeparatorLabel;
}

// Adds or hides ChipButton depending on the 'test' boolean.
- (void)addChipButton:(UIView*)chipButton
          toChipGroup:(NSMutableArray<UIView*>*)chipGroup
               ifTrue:(BOOL)test {
  if (test) {
    [chipGroup addObject:chipButton];
    chipButton.hidden = NO;
  } else {
    chipButton.hidden = YES;
  }
}

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (textView == self.virtualCardInstructionTextView) {
    // The learn more link was clicked.
    [self.navigationDelegate
          openURL:[[CrURL alloc]
                      initWithGURL:autofill::payments::
                                       GetVirtualCardEnrollmentSupportUrl()]
        withTitle:[textView.text substringWithRange:characterRange]];
  }
  return NO;
}

@end
