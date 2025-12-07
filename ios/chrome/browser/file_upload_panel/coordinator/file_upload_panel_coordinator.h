// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the file upload panel UI, which lets the user select files or
// directories from different sources to be submitted to a web page file input.
API_AVAILABLE(ios(18.4))
@interface FileUploadPanelCoordinator : ChromeCoordinator

@end

#endif  // IOS_CHROME_BROWSER_FILE_UPLOAD_PANEL_COORDINATOR_FILE_UPLOAD_PANEL_COORDINATOR_H_
