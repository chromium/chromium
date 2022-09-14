// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_PRESENTATION_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_PRESENTATION_CONTROLLER_DELEGATE_H_

@class TableViewPresentationController;

@protocol TableViewPresentationControllerDelegate

// Returns YES if the presentation controller should dismiss its presented view
// controller when the user touches outside the bounds of the presented view.
- (BOOL)presentationControllerShouldDismissOnTouchOutside:
    (TableViewPresentationController*)controller;

// Informs the delegate that the user took an action that will result in the
// dismissal of the presented view.  It is the delegate's responsibility to call
// `dismissViewController:animated:`.
- (void)presentationControllerWillDismiss:
    (TableViewPresentationController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_PRESENTATION_CONTROLLER_DELEGATE_H_
