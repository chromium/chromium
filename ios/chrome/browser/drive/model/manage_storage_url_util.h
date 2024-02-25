// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_MANAGE_STORAGE_URL_UTIL_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_MANAGE_STORAGE_URL_UTIL_H_

#import <string_view>

class GURL;

// Returns a URL to let user with email `user_email` manage their Drive storage.
GURL GenerateManageDriveStorageUrl(std::string_view user_email);

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_MANAGE_STORAGE_URL_UTIL_H_
