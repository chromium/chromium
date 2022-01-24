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

- (instancetype)initWithFrame:(CGRect)frame;

// Delegate to load urls in the current tab.
@property(nonatomic, weak) id<NewTabPageURLLoaderDelegate> URLLoaderDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_REVAMPED_INCOGNITO_VIEW_H_
