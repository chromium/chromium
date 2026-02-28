// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_INCOGNITO_INCOGNITO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_INCOGNITO_INCOGNITO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol NewTabPageURLLoaderDelegate;

@interface IncognitoViewController : UIViewController

// Delegate to load URLs in the current tab.
@property(nonatomic, weak) id<NewTabPageURLLoaderDelegate> URLLoaderDelegate;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_INCOGNITO_INCOGNITO_VIEW_CONTROLLER_H_
