// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_ACTIVITIES_OPEN_DOWNLOADS_FOLDER_ACTIVITY_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_ACTIVITIES_OPEN_DOWNLOADS_FOLDER_ACTIVITY_H_

#import <UIKit/UIKit.h>

@protocol BrowserCoordinatorCommands;

// Activity that executes "showDownloadsFolder" command.
@interface OpenDownloadsFolderActivity : UIActivity

// Handler used to show the downloaded files manager.
@property(nonatomic, weak) id<BrowserCoordinatorCommands> browserHandler;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_ACTIVITIES_OPEN_DOWNLOADS_FOLDER_ACTIVITY_H_
