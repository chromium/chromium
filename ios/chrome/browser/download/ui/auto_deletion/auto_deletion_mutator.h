// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_MUTATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_MUTATOR_H_

@protocol AutoDeletionMutator <NSObject>

// Enables the auto-deletion feature on the user's device.
- (void)enableAutoDeletion;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_MUTATOR_H_
