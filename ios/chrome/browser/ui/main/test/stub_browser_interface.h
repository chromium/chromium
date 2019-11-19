// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_TEST_STUB_BROWSER_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_MAIN_TEST_STUB_BROWSER_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/main/browser_interface_provider.h"

class Browser;
@class BrowserViewController;
namespace ios {
class ChromeBrowserState;
}
@class TabModel;

// Test double for BrowserInterface implementors. All properties are writable,
// and have nil, nullptr, or NO as default values.
@interface StubBrowserInterface : NSObject <BrowserInterface>
@property(nonatomic, readwrite) UIViewController* viewController;
@property(nonatomic, readwrite) BrowserViewController* bvc;
@property(nonatomic, readwrite) TabModel* tabModel;
@property(nonatomic, readwrite) Browser* browser;
@property(nonatomic, readwrite) ios::ChromeBrowserState* browserState;
@property(nonatomic, readwrite) BOOL incognito;
@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_TEST_STUB_BROWSER_INTERFACE_H_
