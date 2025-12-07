// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_SETTINGS_MUTATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_SETTINGS_MUTATOR_H_

// Protocol that defines the actions the Auto-deletion settings UI can take on
// the model layer.
@protocol AutoDeletionSettingsMutator <NSObject>

// Sets the PrefService value that tracks whether the Downloads Auto-deletion
// feature is enabled or disabled to the value of `status`.
- (void)setDownloadAutoDeletionPermissionStatus:(BOOL)status;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_SETTINGS_MUTATOR_H_
