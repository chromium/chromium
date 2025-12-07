// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_DETAIL_VIEW_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_DETAIL_VIEW_H_

#import <UIKit/UIKit.h>

@class IconDetailView;
@class IconDetailViewConfiguration;

// A protocol for handling `IconDetailView` taps. `-didTapIconDetailView:view`
// will be called when an `IconDetailView` is tapped.
@protocol IconDetailViewTapDelegate

// Indicates that the user has tapped the given `view`.
- (void)didTapIconDetailView:(IconDetailView*)view;

@end

// A view to display an icon, title, description, and (optional) chevron. This
// view can be configured with different layout types to suit various display
// needs.
@interface IconDetailView : UIView

// The object that should receive a message when this view is tapped.
@property(nonatomic, weak) id<IconDetailViewTapDelegate> tapDelegate;

// Unique identifier for the item. Can be `nil`.
@property(nonatomic, copy) NSString* identifier;

// Initializes this view with a `configuration`.
- (instancetype)initWithConfiguration:
    (IconDetailViewConfiguration*)configuration NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_CELLS_ICON_DETAIL_VIEW_H_
