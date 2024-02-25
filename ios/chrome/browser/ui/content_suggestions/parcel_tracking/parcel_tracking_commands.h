// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_COMMANDS_H_

class GURL;

// Command protocol for events for the Parcel Tracking module.
@protocol ParcelTrackingCommands

// Handles a user tap load the `parcelTrackingURL`.
- (void)loadParcelTrackingPage:(GURL)parcelTrackingURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_COMMANDS_H_
