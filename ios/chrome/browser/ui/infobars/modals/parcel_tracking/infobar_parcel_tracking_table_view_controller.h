// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_modal_consumer.h"

@protocol InfobarModalDelegate;
@protocol InfobarParcelTrackingModalDelegate;
@protocol InfobarParcelTrackingPresenter;

// View controller that represents the content for the parcel tracking
// infobar modal.
@interface InfobarParcelTrackingTableViewController
    : LegacyChromeTableViewController <InfobarParcelTrackingModalConsumer>

// Initializes the view controller with the given `delegate` and
// `presenter`.
- (instancetype)
    initWithDelegate:
        (id<InfobarModalDelegate, InfobarParcelTrackingModalDelegate>)delegate
           presenter:(id<InfobarParcelTrackingPresenter>)presenter
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_TABLE_VIEW_CONTROLLER_H_
