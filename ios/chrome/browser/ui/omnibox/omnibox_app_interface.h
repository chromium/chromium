// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Contains the app-side implementation of helpers.
@interface OmniboxAppInterface : NSObject

// Rewrite google URLs to localhost so they can be loaded by the test server.
+ (void)rewriteGoogleURLToLocalhost;

// Forces a variation to be used on the current HTTP header provider. Returns
// YES if the forcing was successful.
+ (BOOL)forceVariationID:(int)variationID;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_APP_INTERFACE_H_
