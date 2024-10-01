// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_cell.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/autofill/core/browser/data_model/credit_card.h"
#import "components/autofill/core/browser/payments/autofill_payments_feature_availability.h"
#import "components/autofill/core/browser/payments/payments_service_url.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/grit/components_scaled_resources.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_card_cell+Testing.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credit_card.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_labeled_chip.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
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

// The UIActions that should be available from the cell's overflow menu button.
@property(nonatomic, strong) NSArray<UIAction*>* menuActions;

// The 0-based index at which the payment method is in the list of payment
// methods to show.
@property(nonatomic, assign) NSInteger cellIndex;

// The part of the cell's accessibility label that is used to indicate the
// 1-based index at which the payment method represented by this item is
// positioned in the list of payment methods to show.
@property(nonatomic, strong) NSString* cellIndexAccessibilityLabel;

@end

@implementation ManualFillCardItem {
  // If `YES`, autofill button is shown for the item.
  BOOL _showAutofillFormButton;
}

- (instancetype)initWithCreditCard:(ManualFillCreditCard*)card
                   contentInjector:
                       (id<ManualFillContentInjector>)contentInjector
                navigationDelegate:(id<CardListDelegate>)navigationDelegate
                       menuActions:(NSArray<UIAction*>*)menuActions
                         cellIndex:(NSInteger)cellIndex
       cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
            showAutofillFormButton:(BOOL)showAutofillFormButton {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _contentInjector = contentInjector;
    _navigationDelegate = navigationDelegate;
    _card = card;
    _menuActions = menuActions;
    _cellIndex = cellIndex;
    _cellIndexAccessibilityLabel = cellIndexAccessibilityLabel;
    _showAutofillFormButton = showAutofillFormButton;
    self.cellClass = [ManualFillCardCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillCardCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithCreditCard:self.card
                  contentInjector:self.contentInjector
               navigationDelegate:self.navigationDelegate
                      menuActions:self.menuActions
                        cellIndex:_cellIndex
      cellIndexAccessibilityLabel:self.cellIndexAccessibilityLabel
           showAutofillFormButton:_showAutofillFormButton];
}
@end

namespace {

// Width of the card icon.
constexpr CGFloat kCardIconWidth = 40;

// Width of the GPay icon.
constexpr CGFloat kGPayIconWidth = 37;

// Returns the last four digits of the card number to be used in an
// accessibility label. The digits need to be split so that VoiceOver will read
// them individually.
NSString* CardNumberLastFourDigits(NSString* obfuscated_number) {
  NSUInteger length = obfuscated_number.length;
  if (length >= 4) {
    NSString* lastFourDigits =
        [obfuscated_number substringFromIndex:length - 4];
    NSMutableArray* digits = [[NSMutableArray alloc] init];
    for (NSUInteger i = 0; i < lastFourDigits.length; i++) {
      [digits addObject:[lastFourDigits substringWithRange:NSMakeRange(i, 1)]];
    }
    return [digits componentsJoinedByString:@" "];
    ;
  }

  return @"";
}

// Helper method to decide whether or not the GPay icon should be shown in the
// cell.
bool ShouldShowGPayIcon(autofill::CreditCard::RecordType card_record_type) {
  switch (card_record_type) {
    case autofill::CreditCard::RecordType::kLocalCard:
      return false;
    case autofill::CreditCard::RecordType::kMaskedServerCard:
    case autofill::CreditCard::RecordType::kVirtualCard:
      return IsKeyboardAccessoryUpgradeEnabled();
    case autofill::CreditCard::RecordType::kFullServerCard:
      // Full server cards are a temporary cached state and are not given as
      // suggestions for manual fill.
      NOTREACHED();
  }
}

// Returns the offset to apply when setting the top anchor constraint of the
// GPay icon as there's some empty space above and under the icon on official
// builds.
CGFloat GPayIconTopAnchorOffset() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return -15;
#else
  return 0;
#endif
}

}  // namespace

@interface ManualFillCardCell () <UITextViewDelegate>

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// The view displayed at the top the cell containing the card icon, the card
// label and an overflow menu button.
@property(nonatomic, strong) UIView* headerView;

// The label with bank name and network.
@property(nonatomic, strong) UILabel* cardLabel;

// The credit card icon.
@property(nonatomic, strong) UIImageView* cardIcon;

// The menu button displayed in the cell's header.
@property(nonatomic, strong) UIButton* overflowMenuButton;

// The text view with instructions for how to use virtual cards.
@property(nonatomic, strong) UITextView* virtualCardInstructionTextView;

// A labeled chip showing the card number.
@property(nonatomic, strong) ManualFillLabeledChip* cardNumberLabeledChip;

// A labeled chip showing the cardholder name.
@property(nonatomic, strong) ManualFillLabeledChip* cardholderLabeledChip;

// A labeled chip showing the card's expiration date.
@property(nonatomic, strong) ManualFillLabeledChip* expirationDateLabeledChip;

// A labeled chip showing the card's CVC.
@property(nonatomic, strong) ManualFillLabeledChip* CVCLabeledChip;

// The content delegate for this item.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

// The navigation delegate for this item.
@property(nonatomic, weak) id<CardListDelegate> navigationDelegate;

// The credit card data for this cell.
@property(nonatomic, weak) ManualFillCreditCard* card;

// Layout guide for the cell's content.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

// Separator line. Used to delimit the header from the rest of the cell.
@property(nonatomic, strong) UIView* headerSeparator;

// Separator line. Used to delimit the virtual card instruction text view from
// the rest of the cell.
@property(nonatomic, strong) UIView* virtualCardInstructionsSeparator;

// Button to autofill the current form with the card's data.
@property(nonatomic, strong) UIButton* autofillFormButton;

// Icon to indicate that the card is a server card.
@property(nonatomic, strong) UIImageView* gPayIcon;

@end

@implementation ManualFillCardCell {
  // The 0-based index at which the payment method is in the list of payment
  // methods to show.
  NSInteger _cellIndex;

  // If `YES`, autofill button is shown for the cell.
  BOOL _showAutofillFormButton;
}

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];

  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

  self.cardLabel.text = @"";

  self.virtualCardInstructionTextView.text = @"";
  self.virtualCardInstructionTextView.hidden = NO;

  [self.cardNumberLabeledChip prepareForReuse];
  [self.expirationDateLabeledChip prepareForReuse];
  [self.cardholderLabeledChip prepareForReuse];
  [self.CVCLabeledChip prepareForReuse];

  self.contentInjector = nil;
  self.navigationDelegate = nil;
  self.cardIcon.image = nil;
  self.card = nil;
  _showAutofillFormButton = NO;
}

- (void)setUpWithCreditCard:(ManualFillCreditCard*)card
                contentInjector:(id<ManualFillContentInjector>)contentInjector
             navigationDelegate:(id<CardListDelegate>)navigationDelegate
                    menuActions:(NSArray<UIAction*>*)menuActions
                      cellIndex:(NSInteger)cellIndex
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton {
  if (!self.dynamicConstraints) {
    self.dynamicConstraints = [[NSMutableArray alloc] init];
  }

  _cellIndex = cellIndex;
  _showAutofillFormButton = showAutofillFormButton;

  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }

  self.contentInjector = contentInjector;
  self.navigationDelegate = navigationDelegate;
  self.card = card;

  [self populateViewsWithCardData:card menuActions:menuActions];
  [self verticallyArrangeViews:card];

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    NSString* accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", cellIndexAccessibilityLabel,
                                   self.cardLabel.attributedText.string];
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    if (ShouldShowGPayIcon(self.card.recordType)) {
      accessibilityLabel =
          [NSString stringWithFormat:@"%@, %@", accessibilityLabel,
                                     l10n_util::GetNSString(
                                         IDS_IOS_AUTOFILL_WALLET_SERVER_NAME)];
    }
#endif
    GiveAccessibilityContextToCellAndButton(
        self.contentView, self.overflowMenuButton, self.autofillFormButton,
        accessibilityLabel);
  }
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  // Holds the views that should be accessible. The ordering in which views are
  // added to this array will reflect the order followed by VoiceOver. When the
  // Keyboard Accessory Upgrade feature is enabled, subviews that need to be
  // read by VoiceOver must be added to this array. Otherwise, they will be
  // ignored.
  NSMutableArray<UIView*>* accessibilityElements =
      [[NSMutableArray alloc] initWithObjects:self.contentView, nil];

  self.layoutGuide =
      AddLayoutGuideToContentView(self.contentView, /*cell_has_header=*/YES);

  self.selectionStyle = UITableViewCellSelectionStyleNone;

  // Create the UIViews, add them to the contentView.
  self.cardLabel = CreateLabel();
  self.cardIcon = [self createCardIcon];
  self.overflowMenuButton = CreateOverflowMenuButton();
  self.headerView =
      CreateHeaderView(self.cardIcon, self.cardLabel, self.overflowMenuButton);
  [self.contentView addSubview:self.headerView];
  [accessibilityElements addObject:self.overflowMenuButton];

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    self.headerSeparator = CreateGraySeparatorForContainer(self.contentView);
  } else {
    // This separator is used to delimit this cell from the others.
    CreateGraySeparatorForContainer(self.contentView);
  }

  UILabel* expirationDateSeparatorLabel;

  // Virtual card instruction textview is always created, but hidden for
  // non-virtual cards.
  self.virtualCardInstructionTextView =
      [self createVirtualCardInstructionTextView];
  [self.contentView addSubview:self.virtualCardInstructionTextView];
  [accessibilityElements addObject:self.virtualCardInstructionTextView];

  self.virtualCardInstructionsSeparator =
      CreateGraySeparatorForContainer(self.contentView);

  self.cardNumberLabeledChip = [[ManualFillLabeledChip alloc]
      initSingleChipWithTarget:self
                      selector:@selector(userDidTapCardNumber:)];
  [self.contentView addSubview:self.cardNumberLabeledChip];
  [accessibilityElements addObject:self.cardNumberLabeledChip.singleButton];

  self.expirationDateLabeledChip = [[ManualFillLabeledChip alloc]
      initExpirationDateChipWithTarget:self
                         monthSelector:@selector(userDidTapExpirationMonth:)
                          yearSelector:@selector(userDidTapExpirationYear:)];
  [self.contentView addSubview:self.expirationDateLabeledChip];
  [accessibilityElements
      addObject:self.expirationDateLabeledChip.expirationMonthButton];
  [accessibilityElements
      addObject:self.expirationDateLabeledChip.expirationYearButton];

  self.cardholderLabeledChip = [[ManualFillLabeledChip alloc]
      initSingleChipWithTarget:self
                      selector:@selector(userDidTapCardholderName:)];
  [self.contentView addSubview:self.cardholderLabeledChip];
  [accessibilityElements addObject:self.cardholderLabeledChip.singleButton];

  self.CVCLabeledChip = [[ManualFillLabeledChip alloc]
      initSingleChipWithTarget:self
                      selector:@selector(userDidTapCVC:)];
  [self.contentView addSubview:self.CVCLabeledChip];
  [accessibilityElements addObject:self.CVCLabeledChip.singleButton];

  self.autofillFormButton = CreateAutofillFormButton();
  [self.contentView addSubview:self.autofillFormButton];
  [accessibilityElements addObject:self.autofillFormButton];
  [self.autofillFormButton addTarget:self
                              action:@selector(onAutofillFormButtonTapped)
                    forControlEvents:UIControlEventTouchUpInside];

  [self horizontallyArrangeViews:expirationDateSeparatorLabel];

  SetUpCellAccessibilityElements(self, accessibilityElements);
}

// Horizontally positions the UIViews.
- (void)horizontallyArrangeViews:(UILabel*)expirationDateSeparatorLabel {
  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];
  AppendHorizontalConstraintsForViews(staticConstraints, @[ self.headerView ],
                                      self.layoutGuide);

  self.gPayIcon = [self createGPayIcon];
  [self.contentView addSubview:self.gPayIcon];

  AppendHorizontalConstraintsForViews(staticConstraints,
                                      @[ self.virtualCardInstructionTextView ],
                                      self.layoutGuide);
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.cardNumberLabeledChip ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide, self.gPayIcon);
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.expirationDateLabeledChip ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.cardholderLabeledChip ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.CVCLabeledChip ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);
  [staticConstraints
      addObject:[self.gPayIcon.topAnchor
                    constraintEqualToAnchor:self.cardNumberLabeledChip.topAnchor
                                   constant:GPayIconTopAnchorOffset()]];

  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.autofillFormButton ], self.layoutGuide);

  if (!IsKeyboardAccessoryUpgradeEnabled()) {
    // Without this set, Voice Over will read the content vertically instead of
    // horizontally.
    self.contentView.shouldGroupAccessibilityChildren = YES;
  }

  [NSLayoutConstraint activateConstraints:staticConstraints];
}

// Adds the data from the ManualFillCreditCard to the corresponding UIViews.
- (void)populateViewsWithCardData:(ManualFillCreditCard*)card
                      menuActions:(NSArray<UIAction*>*)menuActions {
  self.cardIcon.image = card.icon;

  if (menuActions && menuActions.count) {
    self.overflowMenuButton.menu = [UIMenu menuWithChildren:menuActions];
    self.overflowMenuButton.hidden = NO;
  } else {
    self.overflowMenuButton.hidden = YES;
  }

  self.gPayIcon.hidden = !ShouldShowGPayIcon(card.recordType);
  self.gPayIcon.accessibilityIdentifier = [NSString
      stringWithFormat:@"%@ %@", manual_fill::kPaymentManualFillGPayLogoID,
                       card.networkAndLastFourDigits];

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
  if (card.recordType == kVirtualCard) {
    [self.CVCLabeledChip
        setLabelText:
            l10n_util::GetNSString(
                IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_BUBBLE_CVC_LABEL_IOS)
        buttonTitles:@[ card.CVC ]];
  }

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    self.cardNumberLabeledChip.singleButton.accessibilityLabel =
        l10n_util::GetNSStringF(
            IDS_IOS_MANUAL_FALLBACK_CARD_NUMBER_CHIP_ACCESSIBILITY_LABEL,
            base::SysNSStringToUTF16(
                CardNumberLastFourDigits(card.obfuscatedNumber)));
    self.expirationDateLabeledChip.expirationMonthButton.accessibilityLabel =
        l10n_util::GetNSStringF(
            IDS_IOS_MANUAL_FALLBACK_EXPIRATION_MONTH_CHIP_ACCESSIBILITY_LABEL,
            base::SysNSStringToUTF16(card.expirationMonth));
    self.expirationDateLabeledChip.expirationYearButton.accessibilityLabel =
        l10n_util::GetNSStringF(
            IDS_IOS_MANUAL_FALLBACK_EXPIRATION_YEAR_CHIP_ACCESSIBILITY_LABEL,
            base::SysNSStringToUTF16(card.expirationYear));
    self.cardholderLabeledChip.singleButton.accessibilityLabel =
        l10n_util::GetNSStringF(
            IDS_IOS_MANUAL_FALLBACK_CARDHOLDER_CHIP_ACCESSIBILITY_LABEL,
            base::SysNSStringToUTF16(card.cardHolder));
    self.CVCLabeledChip.singleButton.accessibilityLabel =
        l10n_util::GetNSString(
            IDS_IOS_MANUAL_FALLBACK_CVC_CHIP_ACCESSIBILITY_LABEL);
  }
}

// Positions the UIViews vertically.
- (void)verticallyArrangeViews:(ManualFillCreditCard*)card {
  // Holds the views whose leading anchor is constrained relative to the cell's
  // leading anchor.
  std::vector<ManualFillCellView> verticalLeadViews;
  AddViewToVerticalLeadViews(self.headerView,
                             ManualFillCellView::ElementType::kOther,
                             verticalLeadViews);

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    AddViewToVerticalLeadViews(
        self.headerSeparator, ManualFillCellView::ElementType::kHeaderSeparator,
        verticalLeadViews);
  }

  // Holds the chip buttons related to the card that are vertical leads.
  NSMutableArray<UIView*>* cardInfoGroupVerticalLeadChips =
      [[NSMutableArray alloc] init];

  // Virtual card instruction.
  if (card.recordType == kVirtualCard) {
    AddViewToVerticalLeadViews(
        self.virtualCardInstructionTextView,
        ManualFillCellView::ElementType::kVirtualCardInstructions,
        verticalLeadViews);
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      AddViewToVerticalLeadViews(
          self.virtualCardInstructionsSeparator,
          ManualFillCellView::ElementType::kVirtualCardInstructionsSeparator,
          verticalLeadViews);
      self.virtualCardInstructionsSeparator.hidden = NO;
    }
    self.virtualCardInstructionTextView.hidden = NO;
  } else {
    self.virtualCardInstructionTextView.hidden = YES;
    self.virtualCardInstructionsSeparator.hidden = YES;
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

  // CVC labeled chip button.
  [self addChipButton:self.CVCLabeledChip
          toChipGroup:cardInfoGroupVerticalLeadChips
               ifTrue:(card.CVC.length > 0)];

  AddChipGroupsToVerticalLeadViews(@[ cardInfoGroupVerticalLeadChips ],
                                   verticalLeadViews);

  if (_showAutofillFormButton) {
    CHECK(IsKeyboardAccessoryUpgradeEnabled());
    AddViewToVerticalLeadViews(self.autofillFormButton,
                               ManualFillCellView::ElementType::kOther,
                               verticalLeadViews);
    self.autofillFormButton.hidden = NO;
  } else {
    self.autofillFormButton.hidden = YES;
  }

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

  base::RecordAction(
      base::UserMetricsAction([self createMetricsAction:@"SelectCardNumber"]));

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

- (void)userDidTapCVC:(UIButton*)sender {
  CHECK_EQ(self.card.recordType, kVirtualCard);
  base::RecordAction(
      base::UserMetricsAction([self createMetricsAction:@"SelectCvc"]));
  [self.navigationDelegate
      requestFullCreditCard:self.card
                  fieldType:manual_fill::PaymentFieldType::kCVC];
}

// Called when the "Autofill Form" button is tapped. Fills the current form with
// the card's data.
- (void)onAutofillFormButtonTapped {
  base::UmaHistogramSparse(
      "Autofill.UserAcceptedSuggestionAtIndex.CreditCard.ManualFallback",
      _cellIndex);
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_CreditCard_SuggestionAccepted"));

  autofill::SuggestionType type =
              [self.card recordType] == kVirtualCard
          ? autofill::SuggestionType::kVirtualCreditCardEntry
          : autofill::SuggestionType::kCreditCardEntry;
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:nil
                               minorValue:nil
                       displayDescription:nil
                                     icon:nil
                                     type:type
                        backendIdentifier:[self.card GUID]
              fieldByFieldFillingTypeUsed:autofill::EMPTY_TYPE
                           requiresReauth:NO
               acceptanceA11yAnnouncement:
                   base::SysUTF16ToNSString(l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM))];

  [self.contentInjector autofillFormWithSuggestion:suggestion
                                           atIndex:_cellIndex];
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

// Creates and configures the card icon image view.
- (UIImageView*)createCardIcon {
  UIImageView* cardIcon = [[UIImageView alloc] init];
  cardIcon.translatesAutoresizingMaskIntoConstraints = NO;
  [cardIcon setContentHuggingPriority:UILayoutPriorityDefaultHigh
                              forAxis:UILayoutConstraintAxisHorizontal];

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    cardIcon.contentMode = UIViewContentModeScaleAspectFill;
    [cardIcon.widthAnchor constraintEqualToConstant:kCardIconWidth].active =
        YES;
  }

  return cardIcon;
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

// Creates and configures the GPay icon image view.
- (UIImageView*)createGPayIcon {
  UIImage* icon;
  // `kGooglePaySymbol` only exists in official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  icon = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGooglePaySymbol, kGPayIconWidth));
#else
  icon = NativeImage(IDR_AUTOFILL_GOOGLE_PAY);
#endif

  UIImageView* imageView = [[UIImageView alloc] initWithImage:icon];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.contentMode = UIViewContentModeScaleAspectFit;

  [NSLayoutConstraint
      activateConstraints:@[ [imageView.widthAnchor
                              constraintEqualToConstant:kGPayIconWidth] ]];

  return imageView;
}

@end
