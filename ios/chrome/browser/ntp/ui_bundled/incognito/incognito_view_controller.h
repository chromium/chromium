// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_INCOGNITO_INCOGNITO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_INCOGNITO_INCOGNITO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol NewTabPageControllerDelegate;
@protocol PrivacyCookiesCommands;
class UrlLoadingBrowserAgent;

@interface IncognitoViewController : UIViewController

// Init with the given loader object. `loader` may be nil, but isn't
// retained so it must outlive this controller.
// TODO(crbug.com/40228520): View controllers should not have access to
// model-layer objects. Create a mediator to connect model-layer class
// `UrlLoadingBrowserAgent` to the view controller.
- (instancetype)initWithUrlLoader:(UrlLoadingBrowserAgent*)URLLoader;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_INCOGNITO_INCOGNITO_VIEW_CONTROLLER_H_
