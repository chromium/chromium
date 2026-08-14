// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SAVE_TO_DRIVE_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SAVE_TO_DRIVE_COMMANDS_H_

#import <Foundation/Foundation.h>

namespace web {
class DownloadTask;
}

// Commands related to Save to Drive.
@protocol SaveToDriveCommands

// Starts Save to Drive UI for the given download task.
- (void)showSaveToDriveForDownload:(web::DownloadTask*)downloadTask;

// Stops Save to Drive UI.
- (void)hideSaveToDriveAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SAVE_TO_DRIVE_COMMANDS_H_
