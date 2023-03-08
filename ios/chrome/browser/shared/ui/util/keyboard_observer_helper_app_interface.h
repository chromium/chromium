// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_KEYBOARD_OBSERVER_HELPER_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_KEYBOARD_OBSERVER_HELPER_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@class KeyboardObserverHelper;

// Utility to interact with a KeyboardObserverInstance on Earl Grey 2 tests.
@interface KeyboardObserverHelperAppInterface : NSObject

// Returns a shared instance of the observer.
+ (KeyboardObserverHelper*)appSharedInstance;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_KEYBOARD_OBSERVER_HELPER_APP_INTERFACE_H_
