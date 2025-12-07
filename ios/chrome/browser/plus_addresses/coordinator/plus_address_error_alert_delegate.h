// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_COORDINATOR_PLUS_ADDRESS_ERROR_ALERT_DELEGATE_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_COORDINATOR_PLUS_ADDRESS_ERROR_ALERT_DELEGATE_H_

// Delegate used to convey the user decision of plus address error alerts.
@protocol PlusAddressErrorAlertDelegate

// Used to indicate that the user chose to use the existing affiliated plus
// address.
- (void)didAcceptAffiliatedPlusAddressSuggestion;

// Used to indicate that the user chose to cancel the alert.
- (void)didCancelAlert;

// Used to indicate that the user chose to try again the request to confirm the
// plus address.
- (void)didSelectTryAgainToConfirm;

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_COORDINATOR_PLUS_ADDRESS_ERROR_ALERT_DELEGATE_H_
