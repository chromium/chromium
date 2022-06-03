// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_ACTIVITIES_OPEN_DOWNLOADS_FOLDER_ACTIVITY_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_ACTIVITIES_OPEN_DOWNLOADS_FOLDER_ACTIVITY_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_table_view_controller_delegate.h"

@protocol BrowserCoordinatorCommands;

// Activity that executes "showDownloadsFolder" command.
@interface OpenDownloadsFolderActivity : UIActivity

// Handler used to show the downloaded files manager.
@property(nonatomic, weak) id<BrowserCoordinatorCommands> browserHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_ACTIVITIES_OPEN_DOWNLOADS_FOLDER_ACTIVITY_H_
