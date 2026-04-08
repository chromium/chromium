// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_TABLE_VIEW_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AutofillAISaveEntityTableViewController;
@class CrURL;

// Delegate for the AutofillAISaveEntityTableViewController.
@protocol AutofillAISaveEntityTableViewControllerDelegate

// Called when the user taps on a link in the footer.
- (void)didTapLinkWithURL:(CrURL*)url;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AI_UI_AUTOFILL_AI_SAVE_ENTITY_TABLE_VIEW_CONTROLLER_DELEGATE_H_
