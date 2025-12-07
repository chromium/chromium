// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_INCOGNITO_INCOGNITO_VIEW_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_INCOGNITO_INCOGNITO_VIEW_H_

#import <UIKit/UIKit.h>

@protocol NewTabPageURLLoaderDelegate;

// The scrollview containing the views. Its content's size is constrained on its
// superview's size.
@interface IncognitoView : UIScrollView

// Initialize the view with a given `frame`.
// Set `showTopIncognitoImageAndTitle` to `YES` to have the top Incognito
// header (with a big icon and title) added at the top of the scroll view
// content.
// Value `stackViewHorizontalMargin` is used to customize horizontal margins
// on leading and trailing ends of the main stack view.
// Value `stackViewMaxWidth` is used to enforce a maximum width for
// the main stack view.
- (instancetype)initWithFrame:(CGRect)frame
    showTopIncognitoImageAndTitle:(BOOL)showTopIncognitoImageAndTitle
        stackViewHorizontalMargin:(CGFloat)stackViewHorizontalMargin
                stackViewMaxWidth:(CGFloat)stackViewMaxWidth
    NS_DESIGNATED_INITIALIZER;

// Calls the designated initializer with `frame` as frame.
// Sets `showTopIncognitoImageAndTitle` to `YES`,
// `stackViewHorizontalMargin` to `kStackViewHorizontalMargin` and
// `stackViewMaxWidth` to `kStackViewMaxWidth`.
- (instancetype)initWithFrame:(CGRect)frame;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Delegate to load urls in the current tab.
@property(nonatomic, weak) id<NewTabPageURLLoaderDelegate> URLLoaderDelegate;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_INCOGNITO_INCOGNITO_VIEW_H_
