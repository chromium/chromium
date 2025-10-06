// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FILE_UPLOAD_PANEL_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FILE_UPLOAD_PANEL_COMMANDS_H_

#import <Foundation/Foundation.h>

// Commands to show/hide the file upload panel.
@protocol FileUploadPanelCommands <NSObject>

// Shows the file upload panel for the active tab.
- (void)showFileUploadPanel API_AVAILABLE(ios(18.4));

// Hides the file upload panel.
- (void)hideFileUploadPanel API_AVAILABLE(ios(18.4));

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_FILE_UPLOAD_PANEL_COMMANDS_H_
