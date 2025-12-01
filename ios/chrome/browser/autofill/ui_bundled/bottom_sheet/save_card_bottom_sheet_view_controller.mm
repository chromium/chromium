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
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Default spacing used for the views in the bottom sheet.
CGFloat const kSpacing = 10;

// Spacing before the logo in the bottom sheet.
CGFloat const kSpacingBeforeAboveTitleImage = 12;

// Spacing after the logo in the bottom sheet.
CGFloat const kSpacingAfterAboveTitleImage = 4;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
//  Height of the Google Pay logo used as the image above the title of the
//  bottomsheet for upload save.
CGFloat const kGooglePayLogoHeight = 32;

// Height of the Chrome logo used as the image above the title of the
// bottomsheet for local save.
CGFloat const kChromeLogoHeight = 22;
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
  // Accessibility label for the _aboveTitleImage.
  NSString* _aboveTitleImageAccessibilityLabel;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.image = [self aboveTitleImage];
  self.imageViewAccessibilityLabel = [self aboveTitleImageAccessibilityLabel];
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
  _aboveTitleImageAccessibilityLabel = description;
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
  self.configuration.primaryActionString = acceptActionText;
  [self reloadConfiguration];
}

- (void)setCancelActionText:(NSString*)cancelActionText {
  self.configuration.secondaryActionString = cancelActionText;
  [self reloadConfiguration];
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
  [self setLoading:YES];
}

- (void)showConfirmationState {
  BOOL wasLoadingShown = self.configuration.isLoading;
  [self setConfirmed:YES];
  self.primaryActionButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME);

  if (wasLoadingShown) {
    // When transitioning from loading state to confirmation state an
    // accessibility announcement needs to be posted since the
    // primaryActionButton would already be in a disabled state during the
    // loading state. As only its label changes in confirmation state, it would
    // not be announced by VoiceOver. However, when transitioning from normal
    // state directly to confirmation state, posting an accessibility
    // announcement must be avoided to not interfere with the announcement from
    // the primaryActionButton.
    UIAccessibilityPostNotification(
        UIAccessibilityAnnouncementNotification,
        l10n_util::GetNSString(
            IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_ACCESSIBLE_NAME));
  }
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
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView
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
  [TableViewCellContentConfiguration registerCellForTableView:tableView];

  return tableView;
}

- (NSUInteger)rowCount {
  return 1;
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  UITableViewCell* cell = [[UITableViewCell alloc] init];
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

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
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
  //  iOS-specific symbol is used to get an optimized image with better
  //  resolution.
  switch ([self.dataSource logoType]) {
    case kChromeLogo:
      return MakeSymbolMulticolor(CustomSymbolWithPointSize(
          kMulticolorChromeballSymbol, kChromeLogoHeight));
    case kGooglePayLogo:
      return MakeSymbolMulticolor(
          CustomSymbolWithPointSize(kGooglePaySymbol, kGooglePayLogoHeight));
    case kNoLogo:
    default:
      NOTREACHED() << "Unsupported logo type for save card bottomsheet.";
  }
#else
  return _aboveTitleImage;
#endif
}

// Returns the accessibility label to be used for the image to be used above the
// title of the bottomsheet.
- (NSString*)aboveTitleImageAccessibilityLabel {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return [self.dataSource logoAccessibilityLabel];
#else
  return _aboveTitleImageAccessibilityLabel;
#endif
}

// Configures the cell for the table view with information of the card to be
// saved.
- (UITableViewCell*)layoutCell:(UITableViewCell*)cell
             forTableViewWidth:(CGFloat)tableViewWidth
                   atIndexPath:(NSIndexPath*)indexPath {
  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = _cardNameAndLastFourDigits;
  configuration.subtitle = _cardExpiryDate;
  configuration.customAccessibilityLabel = _cardAccessibilityLabel;

  ImageContentConfiguration* imageConfiguration =
      [[ImageContentConfiguration alloc] init];
  imageConfiguration.image = _cardIcon;

  configuration.leadingConfiguration = imageConfiguration;

  cell.contentConfiguration = configuration;

  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  cell.userInteractionEnabled = NO;
  cell.accessibilityIdentifier = _cardNameAndLastFourDigits;

  return cell;
}

@end
