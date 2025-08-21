// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOWNLOAD_LIST_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOWNLOAD_LIST_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands related to Download List UI.
@protocol DownloadListCommands <NSObject>

// Hides the download list UI.
- (void)hideDownloadList;

// Shows the download list UI.
- (void)showDownloadList;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_DOWNLOAD_LIST_COMMANDS_H_
