// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTO_DELETION_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTO_DELETION_COMMANDS_H_

#import <Foundation/Foundation.h>

namespace web {
class DownloadTask;
}  // namespace web

// Protocol for the Auto-deletion commands.
@protocol AutoDeletionCommands <NSObject>

// Displays the Auto-deletion action sheet on the UI.
- (void)presentAutoDeletionActionSheetWithDownloadTask:(web::DownloadTask*)task;

// Hides the Auto-deletion action sheet from the view.
- (void)dismissAutoDeletionActionSheet;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_AUTO_DELETION_COMMANDS_H_
