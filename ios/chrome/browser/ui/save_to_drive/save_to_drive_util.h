// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_UTIL_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_UTIL_H_

#import <Foundation/Foundation.h>

@class AccountPickerConfiguration;

namespace web {
class DownloadTask;
}

namespace drive {

// Returns formatted size string.
NSString* GetSizeString(int64_t size_in_bytes);

// Returns the appropriate account picker body text given `file_name` and
// `file_size`. If `file_size` is negative, then it will not appear in the body
// text.
NSString* GetAccountPickerBodyText(NSString* file_name, int64_t file_size);

// Returns the appropriate account picker configuration for `download_task`.
AccountPickerConfiguration* GetAccountPickerConfiguration(
    web::DownloadTask* download_task);

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_DRIVE_SAVE_TO_DRIVE_UTIL_H_
