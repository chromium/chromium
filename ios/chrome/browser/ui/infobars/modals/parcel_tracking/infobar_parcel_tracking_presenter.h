// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_PRESENTER_H_

// Handles presenting "Report an issue" page.
@protocol InfobarParcelTrackingPresenter

// Called when user has tapped the "Report an issue" button.
- (void)showReportIssueView;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_PARCEL_TRACKING_INFOBAR_PARCEL_TRACKING_PRESENTER_H_
