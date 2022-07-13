// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_REVAMPED_INCOGNITO_VIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_REVAMPED_INCOGNITO_VIEW_H_

#import <UIKit/UIKit.h>

@protocol NewTabPageURLLoaderDelegate;

// The scrollview containing the views. Its content's size is constrained on its
// superview's size.
@interface RevampedIncognitoView : UIScrollView

// Initialize the view with a given `frame`.
// Set `showTopIncognitoImageAndTitle` to `YES` to have the top Incognito
// header (with a big icon and title) added at the top of the scroll view
// content.
- (instancetype)initWithFrame:(CGRect)frame
    showTopIncognitoImageAndTitle:(BOOL)showTopIncognitoImageAndTitle
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Delegate to load urls in the current tab.
@property(nonatomic, weak) id<NewTabPageURLLoaderDelegate> URLLoaderDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_REVAMPED_INCOGNITO_VIEW_H_
