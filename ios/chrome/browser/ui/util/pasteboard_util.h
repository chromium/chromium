// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_PASTEBOARD_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_PASTEBOARD_UTIL_H_

#include <vector>

#import <UIKit/UIKit.h>

class GURL;

// Stores `url` in the pasteboard. `url` must be valid.
void StoreURLInPasteboard(const GURL& url);

// Stores `urls` in the pasteboard. `urls` must not be empty and each url
// within `urls` must be valid. (Use `ClearPasteboard()` explicitly to clear
// existing items.)
void StoreURLsInPasteboard(const std::vector<const GURL>& urls);

// Stores `text` and `url` into the pasteboard.
void StoreInPasteboard(NSString* text, const GURL& url);

// Effectively clears any items in the pasteboard.
void ClearPasteboard();

#endif  // IOS_CHROME_BROWSER_UI_UTIL_PASTEBOARD_UTIL_H_
