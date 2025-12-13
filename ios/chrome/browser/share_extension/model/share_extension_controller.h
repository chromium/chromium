// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_CONTROLLER_H_

#import <Foundation/Foundation.h>

// This class observes the Application group folder
// `app_group::ShareExtensionItemsFolder()` and process the files it contains
// when a new file is created.
@interface ShareExtensionController : NSObject

- (instancetype)init NS_DESIGNATED_INITIALIZER;

// Called when the application did become active (will enter foreground).
- (void)applicationDidBecomeActive;

// Called when the application is terminating.
- (void)applicationWillResignActive;

// Start the processing of old and new files in
// `app_group::ShareExtensionItemsFolder()` (read and parse).
- (void)startFilesProcessing;

// Stop the processing of the files when the app will terminate.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_CONTROLLER_H_
