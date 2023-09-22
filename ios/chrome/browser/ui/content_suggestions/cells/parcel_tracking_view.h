// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_PARCEL_TRACKING_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_PARCEL_TRACKING_VIEW_H_

#import <UIKit/UIKit.h>

@class ParcelTrackingItem;

// View for the Parcel Tracking module.
@interface ParcelTrackingModuleView : UIView

// Initializes and configures the view with `config`.
- (instancetype)initWithConfiguration:(ParcelTrackingItem*)config
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Favicon image.
@property(nonatomic, strong) UIImageView* iconImageView;

// Title of the view.
@property(nonatomic, strong, readonly) UILabel* titleLabel;

// Subtitle of the view.
@property(nonatomic, strong, readonly) UILabel* subtitleLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_PARCEL_TRACKING_VIEW_H_
