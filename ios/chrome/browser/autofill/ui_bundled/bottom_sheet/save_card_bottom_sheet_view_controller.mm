// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/save_card_bottom_sheet_view_controller.h"

#import "build/branding_buildflags.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/bottom_sheet_constants.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Credit card icon corner radius.
CGFloat const kCreditCardIconCornerRadius = 5;

// Default spacing used for the views in the bottom sheet.
CGFloat const kSpacing = 10;

// Spacing before the logo in the bottom sheet.
CGFloat const kSpacingBeforeAboveTitleImage = 12;

// Spacing after the logo in the bottom sheet.
CGFloat const kSpacingAfterAboveTitleImage = 4;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Height of the Google Pay logo used as the image above the title of the
// bottomsheet.
CGFloat const kGooglePayLogoHeight = 32;
#endif

}  // namespace

@interface SaveCardBottomSheetViewController () <ConfirmationAlertActionHandler,
                                                 UITableViewDataSource,
                                                 UITextViewDelegate>
@end

// TODO(crbug.com/391366699): Implement SaveCardBottomSheetViewController.
@implementation SaveCardBottomSheetViewController {
  NSString* _cardNameAndLastFourDigits;
  NSString* _cardExpiryDate;
  UIImage* _cardIcon;
  NSString* _cardAccessibilityLabel;
  NSArray<SaveCardMessageWithLinks*>* _legalMessages;
  // Image to be displayed above the title of the bottomsheet.
  UIImage* _aboveTitleImage;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.image = [self aboveTitleImage];
  self.customSpacingBeforeImageIfNoNavigationBar =
      kSpacingBeforeAboveTitleImage;
  self.customSpacingAfterImage = kSpacingAfterAboveTitleImage;
  self.customSpacing = kSpacing;
  self.actionHandler = self;

  [super viewDidLoad];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [self.delegate onViewDisappeared];
}

#pragma mark - SaveCardBottomSheetConsumer

- (void)setAboveTitleImage:(UIImage*)logoImage {
  _aboveTitleImage = logoImage;
}

- (void)setAboveTitleImageDescription:(NSString*)description {
  self.imageViewAccessibilityLabel = description;
}

- (void)setTitle:(NSString*)title {
  self.titleString = title;
  self.titleTextStyle = UIFontTextStyleTitle2;
}

- (void)setSubtitle:(NSString*)subtitle {
  self.subtitleString = subtitle;
  self.subtitleTextStyle = UIFontTextStyleFootnote;
  self.subtitleTextColor = [UIColor colorNamed:kTextPrimaryColor];
}

- (void)setAcceptActionText:(NSString*)acceptActionText {
  self.primaryActionString = acceptActionText;
}

- (void)setCancelActionText:(NSString*)cancelActionText {
  self.secondaryActionString = cancelActionText;
}

- (void)setLegalMessages:(NSArray<SaveCardMessageWithLinks*>*)legalMessages {
  _legalMessages = legalMessages;
}

- (void)setCardNameAndLastFourDigits:(NSString*)label
                  withCardExpiryDate:(NSString*)subLabel
                         andCardIcon:(UIImage*)issuerIcon
           andCardAccessibilityLabel:(NSString*)accessibilityLabel {
  _cardNameAndLastFourDigits = label;
  _cardExpiryDate = subLabel;
  _cardIcon = issuerIcon;
  _cardAccessibilityLabel = accessibilityLabel;
  [self reloadTableViewData];
}

- (void)showLoadingStateWithAccessibilityLabel:(NSString*)accessibilityLabel {
  self.primaryActionButton.accessibilityLabel = accessibilityLabel;
  self.isLoading = YES;
  self.isConfirmed = NO;
}

- (void)showConfirmationState {
  self.isLoading = NO;
  self.isConfirmed = YES;
  self.primaryActionButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME);
  // Accessibility announcement needs to posted here since VoiceOver wouldn't
  // announce button's accessibility label as button's state does not change.
  // For confirmation state, the button's state is already disabled from
  // previously showing loading state.
  UIAccessibilityPostNotification(
      UIAccessibilityAnnouncementNotification,
      l10n_util::GetNSString(
          IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME));
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [self rowCount];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewDetailIconCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kDetailIconCellIdentifier
                                      forIndexPath:indexPath];
  return [self layoutCell:cell
        forTableViewWidth:tableView.frame.size.width
              atIndexPath:indexPath];
}

#pragma mark - TableViewBottomSheetViewController

- (UIView*)createUnderTitleView {
  UIStackView* underTitleView = [[UIStackView alloc] initWithFrame:CGRectZero];
  underTitleView.axis = UILayoutConstraintAxisVertical;
  underTitleView.spacing = kSpacing;

  [underTitleView addArrangedSubview:[self createTableView]];

  for (SaveCardMessageWithLinks* message in _legalMessages) {
    UITextView* legalMessageTextView =
        [AutofillCreditCardUtil createTextViewForLegalMessage:message];
    legalMessageTextView.delegate = self;
    [underTitleView addArrangedSubview:legalMessageTextView];
  }

  return underTitleView;
}

- (UITableView*)createTableView {
  UITableView* tableView = [super createTableView];

  tableView.dataSource = self;
  [tableView registerClass:TableViewDetailIconCell.class
      forCellReuseIdentifier:kDetailIconCellIdentifier];

  return tableView;
}

- (NSUInteger)rowCount {
  return 1;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  TableViewDetailIconCell* cell = [[TableViewDetailIconCell alloc] init];
  CGFloat tableWidth = [self tableViewWidth];
  cell = [self layoutCell:cell
        forTableViewWidth:[self tableViewWidth]
              atIndexPath:[NSIndexPath indexPathForRow:index inSection:0]];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(tableWidth, 1)].height;
}

#pragma mark - ConfirmationAlertActionHandler

// Accept button was pressed.
- (void)confirmationAlertPrimaryAction {
  [self.mutator didAccept];
}

// Cancel button was pressed.
- (void)confirmationAlertSecondaryAction {
  [self.mutator didCancel];
}

#pragma mark - UITextViewDelegate

#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  // A link in legal message was clicked.
  [self.delegate didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
  return NO;
}
#endif

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction API_AVAILABLE(ios(17.0)) {
  // A link in legal message was clicked.
  __weak __typeof__(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.delegate
        didTapLinkURL:[[CrURL alloc] initWithNSURL:textItem.link]];
  }];
}

#pragma mark - Private

// Returns the image to be used above the title of the bottomsheet.
- (UIImage*)aboveTitleImage {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // iOS-specific symbol is used to get an optimized image with better
  // resolution.
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGooglePaySymbol, kGooglePayLogoHeight));
#else
  return _aboveTitleImage;
#endif
}

// Configures the cell for the table view with information of the card to be
// saved.
- (TableViewDetailIconCell*)layoutCell:(TableViewDetailIconCell*)cell
                     forTableViewWidth:(CGFloat)tableViewWidth
                           atIndexPath:(NSIndexPath*)indexPath {
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  cell.userInteractionEnabled = NO;
  cell.accessibilityIdentifier = _cardNameAndLastFourDigits;
  cell.customAccessibilityLabel = _cardAccessibilityLabel;
  [cell.textLabel setText:_cardNameAndLastFourDigits];
  [cell setDetailText:_cardExpiryDate];
  [cell setIconImage:_cardIcon
            tintColor:nil
      backgroundColor:cell.backgroundColor
         cornerRadius:kCreditCardIconCornerRadius];
  [cell updateIconBackgroundWidthToFitContent:YES];
  [cell setTextLayoutConstraintAxis:UILayoutConstraintAxisVertical];
  cell.separatorInset = [self separatorInsetForTableViewWidth:tableViewWidth
                                                  atIndexPath:indexPath];
  return cell;
}

@end
