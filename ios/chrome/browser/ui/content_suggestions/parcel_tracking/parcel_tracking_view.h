// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_VIEW_H_

#import <UIKit/UIKit.h>

@protocol ContentSuggestionsCommands;
class GURL;
@class ParcelTrackingItem;

// View for the Parcel Tracking module.
@interface ParcelTrackingModuleView : UIView

// Configures this view with `config`.
- (void)configureView:(ParcelTrackingItem*)config;

@property(nonatomic, weak) id<ContentSuggestionsCommands> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_PARCEL_TRACKING_PARCEL_TRACKING_VIEW_H_
