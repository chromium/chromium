// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/plus_addresses/grit/plus_addresses_strings.h"
#import "components/plus_addresses/plus_address_service.h"
#import "components/plus_addresses/plus_address_types.h"
#import "components/plus_addresses/plus_address_ui_utils.h"
#import "components/plus_addresses/settings/plus_address_setting_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_mediator.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_setting_service_factory.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@interface PlusAddressBottomSheetCoordinator () <
    PlusAddressBottomSheetMediatorDelegate>

// Alert coordinator used to show error alerts.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

@end

@implementation PlusAddressBottomSheetCoordinator {
  // The view controller responsible for display of the bottom sheet.
  PlusAddressBottomSheetViewController* _viewController;

  // A mediator that hides data operations from the view controller.
  PlusAddressBottomSheetMediator* _mediator;

  // YES if the `_viewController` is the presenting view controller.
  BOOL _viewIsPresented;

  // The autofill callback to be run if the process completes via confirmation
  // on the bottom sheet.
  plus_addresses::PlusAddressCallback _autofillCallback;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
  plus_addresses::PlusAddressService* plusAddressService =
      PlusAddressServiceFactory::GetForProfile(profile);
  plus_addresses::PlusAddressSettingService* plusAddressSettingService =
      PlusAddressSettingServiceFactory::GetForProfile(profile);
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();

  if (!_autofillCallback) {
    AutofillBottomSheetTabHelper* bottomSheetTabHelper =
        AutofillBottomSheetTabHelper::FromWebState(activeWebState);
    _autofillCallback =
        bottomSheetTabHelper->GetPendingPlusAddressFillCallback();
    CHECK(_autofillCallback);
  }

  _mediator = [[PlusAddressBottomSheetMediator alloc]
      initWithPlusAddressService:plusAddressService
       plusAddressSettingService:plusAddressSettingService
                        delegate:self
                       activeUrl:activeWebState->GetLastCommittedURL()
                       urlLoader:UrlLoadingBrowserAgent::FromBrowser(
                                     self.browser)
                       incognito:self.browser->GetProfile()->IsOffTheRecord()];
  _viewController = [[PlusAddressBottomSheetViewController alloc]
                    initWithDelegate:_mediator
      withBrowserCoordinatorCommands:HandlerForProtocol(
                                         self.browser->GetCommandDispatcher(),
                                         BrowserCoordinatorCommands)];
  // Indicate a preference for half sheet detents, and other styling concerns.
  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _viewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;

  _mediator.consumer = _viewController;

  CHECK(!_viewIsPresented);
  _viewIsPresented = YES;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  // Ensure that the bottom sheet is presented so that it can be dismissed
  // before presenting the alert for error during reserve.
  [_mediator reservePlusAddress];
}

- (void)stop {
  [super stop];
  [self dismissBottomSheetWithCompletion:nil];
}

#pragma mark - PlusAddressBottomSheetMediatorDelegate

- (void)displayPlusAddressAffiliationErrorAlert:
    (const plus_addresses::PlusProfile&)plusProfile {
  NSString* title = l10n_util::GetNSString(
      IDS_PLUS_ADDRESS_AFFILIATION_ERROR_ALERT_TITLE_IOS);
  NSString* message = l10n_util::GetNSStringF(
      IDS_PLUS_ADDRESS_AFFILIATION_ERROR_ALERT_MESSAGE_IOS,
      GetOriginForDisplay(plusProfile),
      base::UTF8ToUTF16(*plusProfile.plus_address));
  NSString* primaryButtonTitle = l10n_util::GetNSString(
      IDS_PLUS_ADDRESS_AFFILIATION_ERROR_PRIMARY_BUTTON_IOS);

  [self showAlertWithTitle:title
                       message:message
            primaryButtonTitle:primaryButtonTitle
          secondaryButtonTitle:l10n_util::GetNSString(IDS_CANCEL)
      shouldDismissBottomSheet:NO
            isAffiliationError:YES];
}

- (void)displayPlusAddressQuotaErrorAlert:(BOOL)shouldDismissBottomSheet {
  NSString* title =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_QUOTA_ERROR_ALERT_TITLE_IOS);
  NSString* message =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_QUOTA_ERROR_ALERT_MESSAGE_IOS);

  [self showAlertWithTitle:title
                       message:message
            primaryButtonTitle:l10n_util::GetNSString(IDS_OK)
          secondaryButtonTitle:nil
      shouldDismissBottomSheet:shouldDismissBottomSheet
            isAffiliationError:NO];
}

- (void)displayPlusAddressTimeoutErrorAlert:(BOOL)shouldDismissBottomSheet {
  NSString* title =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_TIMEOUT_ERROR_ALERT_TITLE_IOS);
  NSString* message =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_TIMEOUT_ERROR_ALERT_MESSAGE_IOS);
  NSString* primaryButtonTitle = l10n_util::GetNSString(
      IDS_PLUS_ADDRESS_ERROR_TRY_AGAIN_PRIMARY_BUTTON_IOS);

  [self showAlertWithTitle:title
                       message:message
            primaryButtonTitle:primaryButtonTitle
          secondaryButtonTitle:l10n_util::GetNSString(IDS_CANCEL)
      shouldDismissBottomSheet:shouldDismissBottomSheet
            isAffiliationError:NO];
}

- (void)displayPlusAddressGenericErrorAlert:(BOOL)shouldDismissBottomSheet {
  NSString* title =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_GENERIC_ERROR_ALERT_TITLE_IOS);
  NSString* message =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_GENERIC_ERROR_ALERT_MESSAGE_IOS);
  NSString* primaryButtonTitle = l10n_util::GetNSString(
      IDS_PLUS_ADDRESS_ERROR_TRY_AGAIN_PRIMARY_BUTTON_IOS);

  [self showAlertWithTitle:title
                       message:message
            primaryButtonTitle:primaryButtonTitle
          secondaryButtonTitle:l10n_util::GetNSString(IDS_CANCEL)
      shouldDismissBottomSheet:shouldDismissBottomSheet
            isAffiliationError:NO];
}

- (void)runAutofillCallback:(NSString*)confirmedPlusAddress {
  std::move(_autofillCallback)
      .Run(base::SysNSStringToUTF8(confirmedPlusAddress));
}

#pragma mark - Private

// Shows an alert view with a `title`, `message` and a `primaryButtonTitle`.
// `secondaryButtonTitle` is optional and can be nil.
// `shouldDismissBottomSheet` if YES, would dismiss the bottom sheet before
// showing an alert. `isAffiliationError` indicates that the error alert is
// shown for an affiliated plus address.
- (void)showAlertWithTitle:(NSString*)title
                     message:(NSString*)message
          primaryButtonTitle:(NSString*)primaryButtonTitle
        secondaryButtonTitle:(NSString*)secondaryButtonTitle
    shouldDismissBottomSheet:(BOOL)shouldDismissBottomSheet
          isAffiliationError:(BOOL)isAffiliationError {
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:shouldDismissBottomSheet
                                     ? self.baseViewController
                                     : _viewController
                         browser:self.browser
                           title:title
                         message:message];

  __weak __typeof__(self) weakSelf = self;
  if (primaryButtonTitle) {
    [self.alertCoordinator
        addItemWithTitle:primaryButtonTitle
                  action:^{
                    if (isAffiliationError) {
                      [weakSelf handlePlusAddressAffiliationErrorAcceptance];
                    } else if (secondaryButtonTitle) {
                      [weakSelf handleTryAgainAction:shouldDismissBottomSheet];
                    } else {
                      [weakSelf handleErrorAlertCancellation];
                    }
                  }
                   style:UIAlertActionStyleDefault];
  }

  if (secondaryButtonTitle) {
    [self.alertCoordinator addItemWithTitle:secondaryButtonTitle
                                     action:^{
                                       [weakSelf handleErrorAlertCancellation];
                                     }
                                      style:UIAlertActionStyleDefault];
  }

  if (shouldDismissBottomSheet) {
    if (_viewIsPresented) {
      [self dismissBottomSheetWithCompletion:^{
        [weakSelf.alertCoordinator start];
      }];
      return;
    }
  }

  [self.alertCoordinator start];
}

// Called if the user accepted the affiliated plus address suggestion.
- (void)handlePlusAddressAffiliationErrorAcceptance {
  [self stopAlertCoordinator];
  [_mediator didAcceptAffiliatedPlusAddressSuggestion];
}

// Called to retry the previous action.
- (void)handleTryAgainAction:(BOOL)didDismissBottomSheet {
  [self stopAlertCoordinator];
  if (didDismissBottomSheet) {
    [self start];
  } else {
    [_mediator didSelectTryAgainToConfirm];
  }
}

// Called when the cancel button ("OK" or "Cancel") is tapped.
- (void)handleErrorAlertCancellation {
  [self stopAlertCoordinator];
  [_mediator didCancelAlert];
}

// Dimisses the alert coordinator.
- (void)stopAlertCoordinator {
  CHECK(self.alertCoordinator);
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

// Dismisses the bottom sheet.
- (void)dismissBottomSheetWithCompletion:(void (^)(void))completion {
  if (_viewIsPresented) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:completion];
    _viewIsPresented = NO;
  }

  _viewController = nil;
  _mediator = nil;
}

@end
