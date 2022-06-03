// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_UTIL_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_UTIL_H_

#import <Foundation/Foundation.h>

class GURL;

// Returns a set of NSStrings that are URL schemes for iTunes Stores.
NSSet<NSString*>* GetItmsSchemes();

// Returns whether |url| has an app store scheme.
bool UrlHasAppStoreScheme(const GURL& url);

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_APP_LAUNCHER_UTIL_H_
