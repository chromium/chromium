// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// PlusAddressAppInterface contains the app-side implementation for helpers.
// These helpers are compiled into the app binary and can be called from either
// app or test code.
@interface PlusAddressAppInterface : NSObject

// Saves an example plus profile.
+ (void)saveExamplePlusProfile:(NSString*)url;

@end

#endif  // IOS_CHROME_BROWSER_PLUS_ADDRESSES_UI_PLUS_ADDRESS_APP_INTERFACE_H_
