// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/coordinator/plus_address_bottom_sheet_mediator.h"

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/plus_addresses/core/browser/metrics/plus_address_metrics.h"
#import "components/plus_addresses/core/browser/plus_address_service.h"
#import "components/plus_addresses/core/browser/plus_address_types.h"
#import "components/plus_addresses/core/browser/plus_address_ui_utils.h"
#import "components/plus_addresses/core/browser/settings/plus_address_setting_service.h"
#import "components/plus_addresses/core/common/features.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_constants.h"
#import "ios/chrome/browser/plus_addresses/ui/plus_address_bottom_sheet_consumer.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

using PlusAddressModalCompletionStatus =
    plus_addresses::metrics::PlusAddressModalCompletionStatus;
using PlusAddressCreationBottomSheetErrorType =
    plus_addresses::PlusAddressCreationBottomSheetErrorType;

PlusAddressCreationBottomSheetErrorType GetCreationErrorType(
    plus_addresses::PlusAddressRequestError request_error) {
  if (request_error.IsQuotaError()) {
    return PlusAddressCreationBottomSheetErrorType::kCreateQuota;
  } else if (request_error.IsTimeoutError()) {
    return PlusAddressCreationBottomSheetErrorType::kCreateTimeout;
  } else {
    return PlusAddressCreationBottomSheetErrorType::kCreateGeneric;
  }
}

}  // namespace

enum class PlusAddressAction {
  kPlusAddressActionReserve = 0,
  kPlusAddressActionConfirm,
  kPlusAddressActionRefresh
};

@implementation PlusAddressBottomSheetMediator {
  // The service implementation that owns the data.
  raw_ptr<plus_addresses::PlusAddressService> _plusAddressService;
  // Manages settings for `PlusAddressService`.
  raw_ptr<plus_addresses::PlusAddressSettingService> _plusAddressSettingService;
  // The origin to which all operations should be scoped.
  url::Origin _mainFrameOrigin;
  // The reserved plus address, which is then eligible for confirmation.
  NSString* _reservedPlusAddress;
  raw_ptr<UrlLoadingBrowserAgent, DanglingUntriaged> _urlLoader;
  BOOL _incognito;

  // The delegate for this mediator.
  __weak id<PlusAddressBottomSheetMediatorDelegate> _delegate;
}

- (instancetype)
    initWithPlusAddressService:(plus_addresses::PlusAddressService*)service
     plusAddressSettingService:
         (plus_addresses::PlusAddressSettingService*)plusAddressSettingService
                      delegate:
                          (id<PlusAddressBottomSheetMediatorDelegate>)delegate
                     activeUrl:(GURL)activeUrl
                     urlLoader:(UrlLoadingBrowserAgent*)urlLoader
                     incognito:(BOOL)incognito {
  // In order to have reached this point, the service should've been created. If
  // not, fail now, since something bad happened.
  CHECK(service);
  self = [super init];
  if (self) {
    _plusAddressService = service;
    _plusAddressSettingService = plusAddressSettingService;
    _delegate = delegate;
    _mainFrameOrigin = url::Origin::Create(activeUrl);
    _urlLoader = urlLoader;
    _incognito = incognito;
  }
  return self;
}

- (void)reservePlusAddress {
  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(^(
      const plus_addresses::PlusProfileOrError& maybePlusProfile) {
    [weakSelf
        handlePlusAddressResult:maybePlusProfile
                      forAction:PlusAddressAction::kPlusAddressActionReserve];
  });

  _plusAddressService->ReservePlusAddress(_mainFrameOrigin,
                                          std::move(callback));
}

#pragma mark - PlusAddressBottomSheetDelegate

- (void)confirmPlusAddress {
  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(^(
      const plus_addresses::PlusProfileOrError& maybePlusProfile) {
    [weakSelf
        handlePlusAddressResult:maybePlusProfile
                      forAction:PlusAddressAction::kPlusAddressActionConfirm];
  });

  _plusAddressService->ConfirmPlusAddress(
      _mainFrameOrigin,
      plus_addresses::PlusAddress(
          base::SysNSStringToUTF8(_reservedPlusAddress)),
      std::move(callback));
}

- (NSString*)primaryEmailAddress {
  std::optional<std::string> primaryAddress =
      _plusAddressService->GetPrimaryEmail();
  // TODO(crbug.com/40276862): determine the appropriate behavior in cases
  // without a primary email (or just switch the signature away from optional).
  if (!primaryAddress.has_value()) {
    return @"";
  }
  return base::SysUTF8ToNSString(primaryAddress.value());
}

- (NSString*)originForDisplay {
  return base::SysUTF16ToNSString(
      plus_addresses::GetOriginForDisplay(_mainFrameOrigin));
}

- (void)openNewTab:(PlusAddressURLType)type {
  UrlLoadParams params = UrlLoadParams::InNewTab([self plusAddressURL:type]);
  params.append_to = OpenPosition::kCurrentTab;
  params.user_initiated = NO;
  params.in_incognito = _incognito;
  _urlLoader->Load(params);
}

- (BOOL)isRefreshEnabled {
  return _plusAddressService->IsRefreshingSupported(_mainFrameOrigin);
}

- (void)didTapRefreshButton {
  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(^(
      const plus_addresses::PlusProfileOrError& maybePlusProfile) {
    [weakSelf
        handlePlusAddressResult:maybePlusProfile
                      forAction:PlusAddressAction::kPlusAddressActionRefresh];
  });
  _plusAddressService->RefreshPlusAddress(_mainFrameOrigin,
                                          std::move(callback));
}

- (BOOL)shouldShowNotice {
  return !_plusAddressSettingService->GetHasAcceptedNotice();
}

#pragma mark - PlusAddressErrorAlertDelegate

- (void)didAcceptAffiliatedPlusAddressSuggestion {
  base::RecordAction(
      base::UserMetricsAction("PlusAddresses.AffiliationErrorFilledExisting"));
  [_delegate runAutofillCallback:_reservedPlusAddress];
  [_consumer dismissBottomSheet];
}

- (void)didCancelAlert {
  [_consumer dismissBottomSheet];
}

- (void)didSelectTryAgainToConfirm {
  base::RecordAction(
      base::UserMetricsAction("PlusAddresses.CreateErrorTryAgainClicked"));
  [_consumer didSelectTryAgainToConfirm];
}

#pragma mark - Private

// Runs the autofill callback and notifies the consumer of the successful
// confirmation.
- (void)runAutofillCallback:(NSString*)confirmedPlusAddress {
  [_delegate runAutofillCallback:confirmedPlusAddress];
  if ([self shouldShowNotice]) {
    _plusAddressSettingService->SetHasAcceptedNotice();
  }
  [_consumer didConfirmPlusAddress];
}

// Once a plus address is successfully reserved, store it and notify the
// consumer.
- (void)didReservePlusAddress:(NSString*)reservedPlusAddress {
  _reservedPlusAddress = reservedPlusAddress;
  [_consumer didReservePlusAddress:reservedPlusAddress];
}

- (GURL)plusAddressURL:(PlusAddressURLType)type {
  switch (type) {
    case PlusAddressURLType::kManagement:
      return GURL(plus_addresses::features::kPlusAddressManagementUrl.Get());
    case PlusAddressURLType::kLearnMore:
      return GURL(plus_addresses::features::kPlusAddressLearnMoreUrl.Get());
  }
}

// This method handles the result of a Plus Address action.
// It takes the `maybePlusProfile` result and the `action` that was performed.
// Based on the action, it calls the appropriate success or error handler.
- (void)handlePlusAddressResult:
            (const plus_addresses::PlusProfileOrError&)maybePlusProfile
                      forAction:(PlusAddressAction)action {
  switch (action) {
      // Both actions have the same success behavior.
    case PlusAddressAction::kPlusAddressActionReserve:
    case PlusAddressAction::kPlusAddressActionRefresh:
      if (maybePlusProfile.has_value()) {
        // If the action was successful, call `didReservePlusAddress` with the
        // reserved Plus Address.
        [self didReservePlusAddress:base::SysUTF8ToNSString(
                                        *maybePlusProfile->plus_address)];
      } else {
        // If the action failed, notify the error.
        [self.consumer notifyError:PlusAddressModalCompletionStatus::
                                       kReservePlusAddressError
               withCreateErrorType:PlusAddressCreationBottomSheetErrorType::
                                       kNoError];
        if (maybePlusProfile.error().IsQuotaError()) {
          [_delegate displayPlusAddressQuotaErrorAlert:YES];
        } else if (maybePlusProfile.error().IsTimeoutError()) {
          [_delegate displayPlusAddressTimeoutErrorAlert:YES];
        } else {
          [_delegate displayPlusAddressGenericErrorAlert:YES];
        }
      }
      break;
    case PlusAddressAction::kPlusAddressActionConfirm:
      if (maybePlusProfile.has_value()) {
        NSString* confirmedPlusAddress =
            base::SysUTF8ToNSString(*maybePlusProfile->plus_address);
        if ([_reservedPlusAddress isEqualToString:confirmedPlusAddress]) {
          // If the action was successful, call `runAutofillCallback` with the
          // confirmed Plus Address.
          [self runAutofillCallback:confirmedPlusAddress];
        } else {
          [self.consumer notifyError:PlusAddressModalCompletionStatus::
                                         kConfirmPlusAddressError
                 withCreateErrorType:PlusAddressCreationBottomSheetErrorType::
                                         kCreateAffiliation];
          _reservedPlusAddress = confirmedPlusAddress;
          // Show affiliation error.
          [_delegate displayPlusAddressAffiliationErrorAlert:*maybePlusProfile];
        }
      } else {
        plus_addresses::PlusAddressRequestError error =
            maybePlusProfile.error();
        // If the action failed, notify the error.
        [self.consumer notifyError:plus_addresses::metrics::
                                       PlusAddressModalCompletionStatus::
                                           kConfirmPlusAddressError
               withCreateErrorType:GetCreationErrorType(error)];
        if (error.IsQuotaError()) {
          [_delegate displayPlusAddressQuotaErrorAlert:NO];
        } else if (error.IsTimeoutError()) {
          [_delegate displayPlusAddressTimeoutErrorAlert:NO];
        } else {
          [_delegate displayPlusAddressGenericErrorAlert:NO];
        }
      }
      break;
  }
}

@end
