// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_TEST_STUB_BROWSER_INTERFACE_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_TEST_STUB_BROWSER_INTERFACE_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/test/stub_browser_interface.h"

// Test double for BrowserInterfaceProvider implementors. All properties are
// writeable. It behaves as follows:
// - It creates two StubBrowserInterfaces on init.
// - `currentInterface` is settable and defaults to the main interface.
// - The current interface is has `current` set to YES on init.
// - The incoignito interface has `incognito` set to YES on init.
// - All other methods are no-ops, and there is no control logic to update the
//   interfaces in any way after init.
@interface StubBrowserInterfaceProvider : NSObject <BrowserInterfaceProvider>

// Specify concrete (stub) implementations for the interfaces, so tests can
// set values on them.
@property(nonatomic, weak, readwrite) StubBrowserInterface* currentInterface;
@property(nonatomic, readwrite) StubBrowserInterface* mainInterface;
@property(nonatomic, readwrite) StubBrowserInterface* incognitoInterface;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_TEST_STUB_BROWSER_INTERFACE_PROVIDER_H_
