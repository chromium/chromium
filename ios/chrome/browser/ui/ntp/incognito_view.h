// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_H_

#import <UIKit/UIKit.h>

class UrlLoadingBrowserAgent;

// The scrollview containing the views. Its content's size is constrained on its
// superview's size.
@interface IncognitoView : UIScrollView

- (instancetype)initWithFrame:(CGRect)frame
                    URLLoader:(UrlLoadingBrowserAgent*)URLLoader;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_H_
