// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_UTILS_H_
#define IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_UTILS_H_

#import <string>

@class URLWithTitle;

// Returns URL and its formatted string representation.
URLWithTitle* GetURLWithTitleForURLString(const std::string& url_string);

#endif  // IOS_CHROME_BROWSER_DATA_IMPORT_PUBLIC_UTILS_H_
