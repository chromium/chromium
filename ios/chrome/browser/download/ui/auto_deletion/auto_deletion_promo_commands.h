// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_PROMO_COMMANDS_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_PROMO_COMMANDS_H_

#import <Foundation/Foundation.h>

// Local command protocol for UI transitions driven by the Auto-deletion promo.
@protocol AutoDeletionPromoCommands <NSObject>

// Dismisses the Auto-deletion promo and presents the enrollment action sheet.
- (void)presentAutoDeletionActionSheet;

// Dismisses the Auto-deletion UI.
- (void)dismissAutoDeletion;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_AUTO_DELETION_AUTO_DELETION_PROMO_COMMANDS_H_
