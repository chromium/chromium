// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_VIEW_H_

#import <UIKit/UIKit.h>

@protocol ParcelTrackingCommands;
class GURL;
@class ParcelTrackingItem;

// View for the Parcel Tracking module.
@interface ParcelTrackingModuleView : UIView

// Configures this view with `config`.
- (void)configureView:(ParcelTrackingItem*)config;

// Command handler for user events.
@property(nonatomic, weak) id<ParcelTrackingCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_VIEW_H_
