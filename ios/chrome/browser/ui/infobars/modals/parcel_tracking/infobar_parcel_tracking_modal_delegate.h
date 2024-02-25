// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_MODAL_DELEGATE_H_

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

// Delegate object that handles model updates according to user action.
@protocol InfobarParcelTrackingModalDelegate <InfobarModalDelegate>

// Called when user has tapped the "track all packages" button.
- (void)parcelTrackingTableViewControllerDidTapTrackAllButton;

// Called when user has tapped the "untrack all packages" button.
- (void)parcelTrackingTableViewControllerDidTapUntrackAllButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_MODAL_DELEGATE_H_
