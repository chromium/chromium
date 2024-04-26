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
#import "components/plus_addresses/plus_address_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_constants.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_delegate.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Generates the description to be displayed in the modal, which includes an
// attributed string that links to the user's myaccount page.
NSAttributedString* DescriptionMessage() {
  // Create and format the text.
  NSDictionary* text_attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  NSString* message = l10n_util::GetNSString(
      IDS_PLUS_ADDRESS_MODAL_PLUS_ADDRESS_DESCRIPTION_IOS);

  NSDictionary* link_attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    // Opening management page is handled by the delegate.
    NSLinkAttributeName : @"",
  };

  return AttributedStringFromStringWithLink(message, text_attributes,
                                            link_attributes);
}

// Generate the error message with link to report error for displaying on the
// bottom sheet.
NSAttributedString* ErrorMessage() {
  NSDictionary* text_attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
  };
  NSString* message = l10n_util::GetNSString(
      IDS_PLUS_ADDRESS_MODAL_REPORT_ERROR_INSTRUCTION_IOS);
  NSDictionary* link_attributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    // Opening error report page is handled by the delegate.
    NSLinkAttributeName : @"",
  };

  return AttributedStringFromStringWithLink(message, text_attributes,
                                            link_attributes);
}

// Returns the image that should be used for the PlusAddress logo.
UIImage* PlusAddressesLogo() {
  // IDR_PLUS_ADDRESS_LOGO only exists in official builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  UIImage* icon = NativeImage(IDR_PLUS_ADDRESS_LOGO);
  // Scale down image size to prevent content overflow.
  if (icon && (icon.size.width > kBrandedImageWidth)) {
    CGFloat ratio = icon.size.width / kBrandedImageWidth;
    return [UIImage imageWithCGImage:[icon CGImage]
                               scale:icon.scale * ratio
                         orientation:icon.imageOrientation];
  }
  return icon;
#else
  return DefaultSymbolTemplateWithPointSize(kMailFillSymbol, kImageSize);
#endif
}

}  // namespace

@interface PlusAddressBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate,
    UITextViewDelegate>
@end

@implementation PlusAddressBottomSheetViewController {
  // The delegate that wraps PlusAddressService operations (reserve, confirm,
  // etc.).
  __weak id<PlusAddressBottomSheetDelegate> _delegate;
  // A commands handler that allows dismissing the bottom sheet.
  __weak id<BrowserCoordinatorCommands> _browserCoordinatorHandler;
  // The label that will display the reserved plus address, once it is ready.
  UILabel* _reservedPlusAddressLabel;
  // The description of plus address that will be displayed on the bottom sheet.
  UITextView* _description;
  // The error message with error report instruction that will be shown when
  // error occurs.
  UITextView* _errorMessage;
  // A loading spinner to indicate to the user that an action is in progress.
  UIActivityIndicatorView* _activityIndicator;
  // Record of the time the bottom sheet is shown.
  base::Time _bottomSheetShownTime;
  // Error that occurred while bottom sheet is showing.
  std::optional<
      plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus>
      _bottomSheetErrorStatus;
}

- (instancetype)initWithDelegate:(id<PlusAddressBottomSheetDelegate>)delegate
    withBrowserCoordinatorCommands:
        (id<BrowserCoordinatorCommands>)browserCoordinatorHandler {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _browserCoordinatorHandler = browserCoordinatorHandler;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  [self setupAboveTitleView];
  self.image = PlusAddressesLogo();
  self.imageHasFixedSize = YES;
  self.customScrollViewBottomInsets = kScrollViewBottomInsets;
  self.titleString = l10n_util::GetNSString(IDS_PLUS_ADDRESS_MODAL_TITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_MODAL_OK_TEXT);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_MODAL_CANCEL_TEXT);
  // Don't show the dismiss bar button (with the secondary button used for
  // canceling), and ensure there is still sufficient space between the top of
  // the bottom sheet content and the top of the sheet. This is especially
  // relevant with larger accessibility text sizes.
  self.showDismissBarButton = NO;
  self.topAlignedLayout = YES;
  self.customSpacingBeforeImageIfNoNavigationBar = kBeforeImageTopMargin;
  self.customSpacingAfterImage = kAfterImageMargin;
  // Set up the label that will indicate the reserved plus address to the user.
  _reservedPlusAddressLabel = [self reservedPlusAddressView:@""];
  NSString* primaryEmailAddress = [_delegate primaryEmailAddress];
  UILabel* primaryAddressLabel =
      [self primaryEmailAddressView:primaryEmailAddress];
  _description = [self descriptionView:DescriptionMessage()];
  _errorMessage = [self errorMessageViewWithMessage:ErrorMessage()];
  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _description, primaryAddressLabel, _reservedPlusAddressLabel, _errorMessage
  ]];
  _errorMessage.hidden = YES;
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = 0;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.layoutMarginsRelativeArrangement = YES;
  verticalStack.layoutMargins = UIEdgeInsetsMake(0, 0, 0, 0);
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [verticalStack setCustomSpacing:kPrimaryAddressBottomMargin
                        afterView:primaryAddressLabel];
  self.underTitleView = verticalStack;
  [super viewDidLoad];
  [self setUpBottomSheetDetents];
  self.actionHandler = self;
  self.presentationController.delegate = self;
  // Disable the primary button until such time as the reservation is complete.
  // If reserving an address fails, we should inform the user and not attempt to
  // fill any fields on the page.
  self.primaryActionButton.enabled = NO;
  [_delegate reservePlusAddress];
  plus_addresses::PlusAddressMetrics::RecordModalEvent(
      plus_addresses::PlusAddressMetrics::PlusAddressModalEvent::kModalShown);
  _bottomSheetShownTime = base::Time::Now();
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  self.primaryActionButton.enabled = NO;
  // Make sure the user perceives that something is happening via a spinner.
  [_activityIndicator startAnimating];
  [_delegate confirmPlusAddress];
  plus_addresses::PlusAddressMetrics::RecordModalEvent(
      plus_addresses::PlusAddressMetrics::PlusAddressModalEvent::
          kModalConfirmed);
}

- (void)confirmationAlertSecondaryAction {
  // The cancel button was tapped, which dismisses the bottom sheet.
  // Call out to the command handler to hide the view and stop the coordinator.
  [self dismiss];
  [_browserCoordinatorHandler dismissPlusAddressBottomSheet];
}

#pragma mark - PlusAddressBottomSheetConsumer

- (void)didReservePlusAddress:(NSString*)plusAddress {
  self.primaryActionButton.enabled = YES;
  _reservedPlusAddressLabel.text = plusAddress;
}

- (void)didConfirmPlusAddress {
  plus_addresses::PlusAddressMetrics::RecordModalShownDuration(
      plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus::
          kModalConfirmed,
      base::Time::Now() - _bottomSheetShownTime);
  [_activityIndicator stopAnimating];
  [_browserCoordinatorHandler dismissPlusAddressBottomSheet];
}

- (void)notifyError:
    (plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus)
        status {
  // With any error, whether during the reservation step or the confirmation
  // step, disable submission of the modal.
  _bottomSheetErrorStatus = status;
  self.primaryActionButton.enabled = NO;
  _reservedPlusAddressLabel.hidden = YES;
  _errorMessage.hidden = NO;
  [_activityIndicator stopAnimating];
  // Resize to accommodate error message.
  [self expandBottomSheet];
}

#pragma mark - UITextViewDelegate

// Handle click on URLs on the bottomsheet.
// TODO(crbug.com/40276862) Add primaryActionForTextItem: when this method is
// deprecated after ios 17 (detail on UITextItem.h).
- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  CHECK(textView == _errorMessage || textView == _description);
  if (textView == _errorMessage) {
    [_delegate openNewTab:PlusAddressURLType::kErrorReport];
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

#pragma mark - Private

// Configures the reserved address view, which allows the user to understand the
// plus address they can confirm use of (or not).
- (UILabel*)reservedPlusAddressView:(NSString*)text {
  UILabel* reservedPlusAddressLabel = [[UILabel alloc] init];
  reservedPlusAddressLabel.text = text;

  // Limit the size of text to avoid truncation.
  reservedPlusAddressLabel.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleTitle2, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryExtraExtraExtraLarge);

  reservedPlusAddressLabel.numberOfLines = 0;
  reservedPlusAddressLabel.textAlignment = NSTextAlignmentCenter;
  return reservedPlusAddressLabel;
}

// The primary email address is displayed in a separate view with slightly
// different formatting.
- (UILabel*)primaryEmailAddressView:(NSString*)primaryEmailAddress {
  UILabel* primaryEmailAddressLabel = [[UILabel alloc] init];
  primaryEmailAddressLabel.text = primaryEmailAddress;

  UIFontDescriptor* descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline];
  // Use a bold font for the primary address.
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* fontMetrics =
      [UIFontMetrics metricsForTextStyle:UIFontTextStyleSubheadline];
  primaryEmailAddressLabel.font = [fontMetrics scaledFontForFont:font];

  primaryEmailAddressLabel.numberOfLines = 0;
  primaryEmailAddressLabel.textAlignment = NSTextAlignmentCenter;
  return primaryEmailAddressLabel;
}

// Create a description UITextView, which will describe the function of the
// feature and link out to the user's account settings.
- (UITextView*)descriptionView:(NSAttributedString*)description {
  UITextView* descriptionView = CreateUITextViewWithTextKit1();
  descriptionView.accessibilityIdentifier =
      kPlusAddressModalDescriptionAccessibilityIdentifier;
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

- (UITextView*)errorMessageViewWithMessage:(NSAttributedString*)message {
  UITextView* errorMessageView = CreateUITextViewWithTextKit1();
  errorMessageView.accessibilityIdentifier =
      kPlusAddressModalErrorMessageAccessibilityIdentifier;
  errorMessageView.scrollEnabled = NO;
  errorMessageView.editable = NO;
  errorMessageView.delegate = self;
  errorMessageView.backgroundColor = [UIColor clearColor];
  errorMessageView.adjustsFontForContentSizeCategory = YES;
  errorMessageView.translatesAutoresizingMaskIntoConstraints = NO;
  errorMessageView.textContainerInset = UIEdgeInsetsZero;
  errorMessageView.attributedText = message;
  errorMessageView.textAlignment = NSTextAlignmentCenter;
  return errorMessageView;
}

- (void)setupAboveTitleView {
  _activityIndicator = [[UIActivityIndicatorView alloc]
      initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];

  // Create a container view such that the activity indicator showing doesn't
  // cause the layout to jump.
  UIView* container = [[UIView alloc] initWithFrame:CGRectZero];
  [container addSubview:_activityIndicator];
  _activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  container.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(container, _activityIndicator);
  self.aboveTitleView = container;
}

- (void)dismiss {
  plus_addresses::PlusAddressMetrics::RecordModalEvent(
      plus_addresses::PlusAddressMetrics::PlusAddressModalEvent::
          kModalCanceled);
  if (_bottomSheetErrorStatus.has_value()) {
    plus_addresses::PlusAddressMetrics::RecordModalShownDuration(
        _bottomSheetErrorStatus.value(),
        base::Time::Now() - _bottomSheetShownTime);
  } else {
    plus_addresses::PlusAddressMetrics::RecordModalShownDuration(
        plus_addresses::PlusAddressMetrics::PlusAddressModalCompletionStatus::
            kModalCanceled,
        base::Time::Now() - _bottomSheetShownTime);
  }
  [_browserCoordinatorHandler dismissPlusAddressBottomSheet];
}

@end
