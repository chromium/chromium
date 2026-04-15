// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_SNACKBAR_PRESENTER_H_
#define IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_SNACKBAR_PRESENTER_H_

#import <Foundation/Foundation.h>

// Protocol for presenting snackbars from the TabPicker component.
@protocol TabPickerSnackbarPresenter <NSObject>

// Shows a snackbar indicating that the tab attachment limit has been reached.
- (void)showSnackbarForTabAttachmentLimit:(NSUInteger)attachmentLimit;

// Shows a snackbar with a generic error message if a tab can't be reloaded.
- (void)showCannotReloadTabError;

@end

#endif  // IOS_CHROME_BROWSER_TAB_PICKER_COORDINATOR_TAB_PICKER_SNACKBAR_PRESENTER_H_
