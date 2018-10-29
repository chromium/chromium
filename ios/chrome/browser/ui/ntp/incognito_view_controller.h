// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/web_state/ui/crw_native_content.h"

@protocol NewTabPageControllerDelegate;
@protocol UrlLoader;

@interface IncognitoViewController : UIViewController<CRWNativeContent>

// Init with the given loader object. |loader| may be nil, but isn't
// retained so it must outlive this controller.
- (id)initWithLoader:(id<UrlLoader>)loader;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_VIEW_CONTROLLER_H_
