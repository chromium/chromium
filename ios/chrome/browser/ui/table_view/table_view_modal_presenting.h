// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_MODAL_PRESENTING_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_MODAL_PRESENTING_H_

// TableViewModalPresenting provides methods that allow presented UI to modify
// whether their presentations are modal or not.
@protocol TableViewModalPresenting

// Tells the delegate how to handle touches that are outside the bounds of the
// presented UI.  If set to YES, presentations will be dismissed when the user
// touches outside of the table view's bounds.  Callers should set this to NO if
// user-edited data is currently visible on screen, which could potentially
// otherwise be lost during an inadvertent touch.  If a `transitionCoordinator`
// is provided, any changes in UI will be animated alongside that transition.
// Otherwise, UI changes will be made immediately without animation.
- (void)setShouldDismissOnTouchOutside:(BOOL)shouldDismiss
             withTransitionCoordinator:
                 (id<UIViewControllerTransitionCoordinator>)
                     transitionCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_MODAL_PRESENTING_H_
