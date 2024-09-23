// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_UTIL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_UTIL_H_

#import <string_view>
#import <vector>

// Returns the list of MIME types contained in `accept_attribute`.
std::vector<std::string> ParseAcceptAttributeMimeTypes(
    std::string_view accept_attribute);

// Returns the list of file extensions contained in `accept_attribute`.
std::vector<std::string> ParseAcceptAttributeFileExtensions(
    std::string_view accept_attribute);

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_UTIL_H_
