// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_view_controller.h"

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/types/expected.h"
#import "build/branding_buildflags.h"
#import "components/grit/components_resources.h"
#import "components/plus_addresses/grit/plus_addresses_strings.h"
#import "components/plus_addresses/metrics/plus_address_metrics.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_constants.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using PlusAddressModalCompletionStatus =
    plus_addresses::metrics::PlusAddressModalCompletionStatus;

// Generates the notice to be displayed in the bottomsheet, which includes an
// attributed string.
NSAttributedString* NoticeMessage(NSString* primaryEmailAddress) {
  // Create and format the text.
  NSDictionary* text_attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  NSString* message =
      l10n_util::GetNSStringF(IDS_PLUS_ADDRESS_BOTTOMSHEET_NOTICE_IOS,
                              base::SysNSStringToUTF16(primaryEmailAddress));

  NSDictionary* link_attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle),
    // Opening notice page is handled by the delegate.
    NSLinkAttributeName : @"",
  };

  return AttributedStringFromStringWithLink(message, text_attributes,
                                            link_attributes);
}

// Generates the description to be displayed in the bottomsheet when the notice
// is presented.
NSAttributedString* DescriptionMessageOnNoticeDisplayed() {
  // Create and format the text.
  NSDictionary* text_attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  NSString* message = l10n_util::GetNSString(
      IDS_PLUS_ADDRESS_BOTTOMSHEET_DESCRIPTION_NOTICE_SCREEN);

  return [[NSMutableAttributedString alloc] initWithString:message
                                                attributes:text_attributes];
}

// Generates the description to be displayed in the bottomsheet that contains
// the email.
NSAttributedString* DescriptionMessageWithEmail(NSString* primaryEmailAddress) {
  // Create and format the text.
  NSDictionary* text_attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  NSString* message =
      l10n_util::GetNSStringF(IDS_PLUS_ADDRESS_BOTTOMSHEET_DESCRIPTION_IOS,
                              base::SysNSStringToUTF16(primaryEmailAddress));

  return [[NSMutableAttributedString alloc] initWithString:message
                                                attributes:text_attributes];
}

// Returns the image view with the branding image.
UIImageView* BrandingImageView() {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  // Branding icon inside the container with the white background.
  return [[UIImageView alloc]
      initWithImage:MakeSymbolMulticolor(CustomSymbolWithPointSize(
                        kGoogleIconSymbol, kPlusAddressSheetBrandingIconSize))];
#else
  return [[UIImageView alloc]
      initWithImage:DefaultSymbolTemplateWithPointSize(
                        kMailFillSymbol, kPlusAddressSheetBrandingIconSize)];
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
}

}  // namespace

@interface PlusAddressBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate,
    UITableViewDataSource,
    UITableViewDelegate,
    UITextViewDelegate>
@end

@implementation PlusAddressBottomSheetViewController {
  // The delegate that wraps PlusAddressService operations (reserve, confirm,
  // etc.).
  __weak id<PlusAddressBottomSheetDelegate> _delegate;
  // A commands handler that allows dismissing the bottom sheet.
  __weak id<BrowserCoordinatorCommands> _browserCoordinatorHandler;
  // The reserved plus address label, once it is ready.
  NSString* _reservedPlusAddress;
  // The table view that displays the reserved plus address for confirmation.
  UITableView* _reservedPlusAddressTableView;
  // The description of plus address that will be displayed on the bottom sheet.
  UITextView* _description;
  // Record of the time the bottom sheet is shown.
  base::Time _bottomSheetShownTime;
  // Error that occurred while bottom sheet is showing.
  std::optional<PlusAddressModalCompletionStatus> _bottomSheetErrorStatus;
  // Keeps track of the number of times the refresh button was hit.
  NSInteger _refreshCount;
  // The notice message if it will be shown.
  UITextView* _noticeMessage;
  // A boolean that is set to `YES` when generating a plus address either in the
  // initial state or during the refresh state.
  BOOL _isGenerating;
}

- (instancetype)initWithDelegate:(id<PlusAddressBottomSheetDelegate>)delegate
    withBrowserCoordinatorCommands:
        (id<BrowserCoordinatorCommands>)browserCoordinatorHandler {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _browserCoordinatorHandler = browserCoordinatorHandler;
    _reservedPlusAddress = l10n_util::GetNSString(
        IDS_PLUS_ADDRESS_BOTTOMSHEET_LOADING_TEMPORARY_LABEL_CONTENT_IOS);
    _refreshCount = 0;
    _isGenerating = YES;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.aboveTitleView = [self brandingIconView];
  self.titleString =
      l10n_util::GetNSString([_delegate shouldShowNotice]
                                 ? IDS_PLUS_ADDRESS_BOTTOMSHEET_TITLE_NOTICE_IOS
                                 : IDS_PLUS_ADDRESS_BOTTOMSHEET_TITLE_IOS);
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.primaryActionString =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_BOTTOMSHEET_OK_TEXT_IOS);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_BOTTOMSHEET_CANCEL_TEXT_IOS);
  self.customScrollViewBottomInsets = 0;

  // Don't show the dismiss bar button (with the secondary button used for
  // canceling), and ensure there is still sufficient space between the top of
  // the bottom sheet content and the top of the sheet. This is especially
  // relevant with larger accessibility text sizes.
  self.showDismissBarButton = NO;
  self.topAlignedLayout = YES;
  self.customSpacingBeforeImageIfNoNavigationBar =
      kPlusAddressSheetBeforeImageTopMargin;
  self.customSpacingAfterImage = kPlusAddressSheetAfterImageMargin;

  self.underTitleView = [self setUpUnderTitleView];
  [super viewDidLoad];
  [self setUpBottomSheetDetents];
  self.actionHandler = self;
  self.presentationController.delegate = self;
  // Disable the primary button until such time as the reservation is complete.
  // If reserving an address fails, we should inform the user and not attempt to
  // fill any fields on the page.
  [self enablePrimaryActionButton:NO];
  _bottomSheetShownTime = base::Time::Now();
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self willConfirmPlusAddress];
}

- (void)confirmationAlertSecondaryAction {
  // The cancel button was tapped, which dismisses the bottom sheet.
  // Call out to the command handler to hide the view and stop the coordinator.
  [self dismiss];
  [_browserCoordinatorHandler dismissPlusAddressBottomSheet];
}

#pragma mark - PlusAddressBottomSheetConsumer

- (void)didReservePlusAddress:(NSString*)plusAddress {
  [self enablePrimaryActionButton:YES];
    _isGenerating = NO;
    if (!_refreshCount) {
      plus_addresses::metrics::RecordModalEvent(
          plus_addresses::metrics::PlusAddressModalEvent::kModalShown,
          [_delegate shouldShowNotice]);
    }
  _reservedPlusAddress = plusAddress;
  [_reservedPlusAddressTableView reloadData];
}

- (void)didConfirmPlusAddress {
  plus_addresses::metrics::RecordModalShownOutcome(
      PlusAddressModalCompletionStatus::kModalConfirmed,
      base::Time::Now() - _bottomSheetShownTime,
      /*refresh_count=*/(int)_refreshCount, [_delegate shouldShowNotice]);

  self.isLoading = NO;
  [_browserCoordinatorHandler dismissPlusAddressBottomSheet];
}

- (void)notifyError:(PlusAddressModalCompletionStatus)status {
  _bottomSheetErrorStatus = status;
  self.isLoading = NO;
}

- (void)dismissBottomSheet {
  [self dismiss];
}

- (void)didSelectTryAgainToConfirm {
  [self willConfirmPlusAddress];
}

#pragma mark - UITextViewDelegate

// Handle click on URLs on the bottomsheet.
// TODO(crbug.com/40276862) Add primaryActionForTextItem: when this method is
// deprecated after ios 17 (detail on UITextItem.h).
- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  CHECK(textView == _description);
  if (textView == _noticeMessage) {
    [_delegate openNewTab:PlusAddressURLType::kLearnMore];
  } else {
    [_delegate openNewTab:PlusAddressURLType::kManagement];
  }
  [_browserCoordinatorHandler dismissPlusAddressBottomSheet];
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // TODO(crbug.com/40276862): separate out the cancel click from other exit
  // patterns, on all platforms.
  [self dismiss];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return 1;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  PlusAddressSuggestionLabelCell* cell =
      DequeueTableViewCell<PlusAddressSuggestionLabelCell>(tableView);

  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  BOOL shouldShowRefresh = [_delegate isRefreshEnabled];

  if (_isGenerating) {
    shouldShowRefresh = NO;
    [cell showActivityIndicator];
  } else {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  [cell setLeadingIconImage:CustomSymbolTemplateWithPointSize(
                                kGooglePlusAddressSymbol,
                                kPlusAddressSheetCellImageSize)
              withTintColor:[UIColor colorNamed:kTextSecondaryColor]];
#else
  [cell setLeadingIconImage:DefaultSymbolTemplateWithPointSize(
                                kMailFillSymbol, kPlusAddressSheetCellImageSize)
              withTintColor:[UIColor colorNamed:kTextSecondaryColor]];
#endif
  [cell hideActivityIndicator];
  }

  if (shouldShowRefresh) {
    [cell setTrailingButtonImage:CustomSymbolTemplateWithPointSize(
                                     kArrowClockWiseSymbol,
                                     kPlusAddressSheetCellImageSize)
                   withTintColor:[UIColor colorNamed:kBlueColor]
         accessibilityIdentifier:
             kPlusAddressRefreshButtonAccessibilityIdentifier];
  }

  cell.textLabel.text = _reservedPlusAddress;
  cell.textLabel.accessibilityIdentifier =
      kPlusAddressLabelAccessibilityIdentifier;
  cell.delegate = self;

  return cell;
}

#pragma mark - PlusAddressSuggestionLabelDelegate

- (void)didTapTrailingButton {
  _refreshCount++;
  [self enablePrimaryActionButton:NO];
  _isGenerating = YES;
  _reservedPlusAddress = l10n_util::GetNSString(
      IDS_PLUS_ADDRESS_BOTTOMSHEET_LOADING_TEMPORARY_LABEL_CONTENT_IOS);
  [_reservedPlusAddressTableView reloadData];

  [_delegate didTapRefreshButton];
}

#pragma mark - Private
// Configures the reserved address view, which allows the user to understand the
// plus address they can confirm use of (or not).
- (UITableView*)reservedPlusAddressView {
  UITableView* tableViewContainer =
      [[UITableView alloc] initWithFrame:CGRectZero];
  tableViewContainer.rowHeight = kPlusAddressSheetTableViewCellHeight;
  tableViewContainer.separatorStyle = UITableViewCellSeparatorStyleNone;
  tableViewContainer.layer.cornerRadius =
      kPlusAddressSheetTableViewCellCornerRadius;
  RegisterTableViewCell<PlusAddressSuggestionLabelCell>(tableViewContainer);
  tableViewContainer.dataSource = self;
  tableViewContainer.delegate = self;
  [tableViewContainer.heightAnchor
      constraintEqualToConstant:kPlusAddressSheetTableViewCellHeight]
      .active = YES;
  return tableViewContainer;
}

// Create a description UITextView, which will describe the function of the
// feature and link out to the user's account settings.
- (UITextView*)descriptionView:(NSAttributedString*)description {
  UITextView* descriptionView = CreateUITextViewWithTextKit1();
  descriptionView.accessibilityIdentifier =
      kPlusAddressSheetDescriptionAccessibilityIdentifier;
  descriptionView.scrollEnabled = NO;
  descriptionView.editable = NO;
  descriptionView.delegate = self;
  descriptionView.backgroundColor = [UIColor clearColor];
  descriptionView.adjustsFontForContentSizeCategory = YES;
  descriptionView.translatesAutoresizingMaskIntoConstraints = NO;
  descriptionView.textContainerInset = UIEdgeInsetsZero;
  descriptionView.attributedText = description;
  descriptionView.textAlignment = NSTextAlignmentCenter;
  return descriptionView;
}

- (UITextView*)noticeMessageViewWithMessage:(NSAttributedString*)message {
  UITextView* noticeMessageView = CreateUITextViewWithTextKit1();
  noticeMessageView.accessibilityIdentifier =
      kPlusAddressSheetNoticeMessageAccessibilityIdentifier;
  noticeMessageView.scrollEnabled = NO;
  noticeMessageView.editable = NO;
  noticeMessageView.delegate = self;
  noticeMessageView.backgroundColor = [UIColor clearColor];
  noticeMessageView.adjustsFontForContentSizeCategory = YES;
  noticeMessageView.translatesAutoresizingMaskIntoConstraints = NO;
  noticeMessageView.textContainerInset = UIEdgeInsetsZero;
  noticeMessageView.attributedText = message;
  noticeMessageView.textAlignment = NSTextAlignmentCenter;
  return noticeMessageView;
}

- (UIView*)setUpUnderTitleView {
  // Set up the view that will indicate the reserved plus address to the user
  // for confirmation.
  NSString* email = [_delegate primaryEmailAddress];
  BOOL showNotice = [_delegate shouldShowNotice];
  _reservedPlusAddressTableView = [self reservedPlusAddressView];
  _description =
      [self descriptionView:(showNotice ? DescriptionMessageOnNoticeDisplayed()
                                        : DescriptionMessageWithEmail(email))];
  _noticeMessage =
      [self noticeMessageViewWithMessage:NoticeMessage(
                                             [_delegate primaryEmailAddress])];

  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _description, _reservedPlusAddressTableView, _noticeMessage
  ]];
  _noticeMessage.hidden = !showNotice;
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = 0;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.layoutMarginsRelativeArrangement = YES;
  verticalStack.layoutMargins = UIEdgeInsetsMake(0, 0, 0, 0);
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [verticalStack setCustomSpacing:kPlusAddressSheetPrimaryAddressBottomMargin
                        afterView:_description];
  if (showNotice) {
    [verticalStack setCustomSpacing:kPlusAddressSheetPrimaryAddressBottomMargin
                          afterView:_reservedPlusAddressTableView];
  }
  return verticalStack;
}

- (void)dismiss {
  const bool was_notice_shown = [_delegate shouldShowNotice];
  plus_addresses::metrics::RecordModalEvent(
      plus_addresses::metrics::PlusAddressModalEvent::kModalCanceled,
      was_notice_shown);
  plus_addresses::metrics::RecordModalShownOutcome(
      _bottomSheetErrorStatus.value_or(
          PlusAddressModalCompletionStatus::kModalCanceled),
      base::Time::Now() - _bottomSheetShownTime,
      /*refresh_count=*/(int)_refreshCount, was_notice_shown);
  [_browserCoordinatorHandler dismissPlusAddressBottomSheet];
}

// Returns a view of a branding icon with a white background with vertical
// padding.
- (UIView*)brandingIconView {
  // Container of the trash icon that has the red background.
  UIView* iconContainerView = [[UIView alloc] init];
  iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  iconContainerView.layer.cornerRadius =
      kPlusAddressSheetBrandingIconContainerViewCornerRadius;
  iconContainerView.layer.shadowRadius =
      kPlusAddressSheetBrandingIconContainerViewShadowRadius;
  iconContainerView.layer.shadowOpacity =
      kPlusAddressSheetBrandingIconContainerViewShadowOpacity;
  iconContainerView.backgroundColor = [UIColor colorNamed:kSolidWhiteColor];

  UIImageView* icon = BrandingImageView();
  icon.clipsToBounds = YES;
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  [iconContainerView addSubview:icon];

  [NSLayoutConstraint activateConstraints:@[
    [iconContainerView.widthAnchor
        constraintEqualToConstant:
            kPlusAddressSheetBrandingIconContainerViewSize],
    [iconContainerView.heightAnchor
        constraintEqualToConstant:
            kPlusAddressSheetBrandingIconContainerViewSize],
  ]];
  AddSameCenterConstraints(iconContainerView, icon);

  // Padding for the icon container view.
  UIView* outerView = [[UIView alloc] init];
  [outerView addSubview:iconContainerView];
  AddSameCenterXConstraint(outerView, iconContainerView);
  AddSameConstraintsToSidesWithInsets(
      iconContainerView, outerView, LayoutSides::kTop | LayoutSides::kBottom,
      NSDirectionalEdgeInsetsMake(
          kPlusAddressSheetBrandingIconContainerViewTopPadding, 0,
          kPlusAddressSheetBrandingIconContainerViewBottomPadding, 0));

  return outerView;
}

// Called when the user chose to confirm the plus address.
- (void)willConfirmPlusAddress {
  [self enablePrimaryActionButton:NO];
  self.isLoading = YES;

  [_delegate confirmPlusAddress];
  plus_addresses::metrics::RecordModalEvent(
      plus_addresses::metrics::PlusAddressModalEvent::kModalConfirmed,
      [_delegate shouldShowNotice]);
}

// Enables/Disables the primary action button.
- (void)enablePrimaryActionButton:(BOOL)enabled {
  self.primaryActionButton.enabled = enabled;
  UpdateButtonColorOnEnableDisable(self.primaryActionButton);
}

@end
