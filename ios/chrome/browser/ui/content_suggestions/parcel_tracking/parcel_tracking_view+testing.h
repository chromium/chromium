// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_VIEW_TESTING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_VIEW_TESTING_H_

#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_view.h"

// Category for exposing internal state for testing.
@interface ParcelTrackingModuleView (ForTesting)

- (NSString*)titleLabelTextForTesting;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_VIEW_TESTING_H_
