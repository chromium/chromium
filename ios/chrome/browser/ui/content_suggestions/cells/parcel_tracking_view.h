// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_PARCEL_TRACKING_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_PARCEL_TRACKING_VIEW_H_

#import <UIKit/UIKit.h>

class GURL;
@class ParcelTrackingItem;

// Delegate for ParcelTrackingModuleView.
@protocol ParcelTrackingViewDelegate

// Indicates to the receiver to load the `parcelTrackingURL`.
- (void)loadParcelTrackingPage:(GURL)parcelTrackingURL;

@end

// View for the Parcel Tracking module.
@interface ParcelTrackingModuleView : UIView

// Configures this view with `config`.
- (void)configureView:(ParcelTrackingItem*)config;

@property(nonatomic, weak) id<ParcelTrackingViewDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_PARCEL_TRACKING_VIEW_H_
