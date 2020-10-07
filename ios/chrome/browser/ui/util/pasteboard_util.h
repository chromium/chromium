// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_PASTEBOARD_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_PASTEBOARD_UTIL_H_

#import <UIKit/UIKit.h>

class GURL;

// Stores |url| into the pasteboard.
void StoreURLInPasteboard(const GURL& url);

// Stores |text| and |url| into the pasteboard.
void StoreInPasteboard(NSString* text, const GURL& url);

// Effectively clears any items in the pasteboard.
void ClearPasteboard();

#endif  // IOS_CHROME_BROWSER_UI_UTIL_PASTEBOARD_UTIL_H_
