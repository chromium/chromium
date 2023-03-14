// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARE_DOWNLOAD_OVERLAY_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARE_DOWNLOAD_OVERLAY_COMMANDS_H_

// Commands related to the download overlay UI use before showing sharing menu
// when a download is performed. It displays an overlay view on top of the web
// view. When an user taps on the screen, this overlay should be dismissed.
@protocol ShareDownloadOverlayCommands

// Called when user taps to cancel the download.
- (void)cancelDownload;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARE_DOWNLOAD_OVERLAY_COMMANDS_H_
