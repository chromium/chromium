// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_VIEW_CONTROLLER_DELEGATE_H_

@class PopupMenuViewController;

// Delegate for the view controller.
@protocol PopupMenuViewControllerDelegate

// Tells the delegate that user took an action that will result in the dismissal
// of `viewController`. It is the delegate's responsibility to actually dismiss
// `viewController`.
- (void)popupMenuViewControllerWillDismiss:
    (PopupMenuViewController*)viewController;

- (void)containedViewControllerContentSizeChangedForPopupMenuViewController:
    (PopupMenuViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_VIEW_CONTROLLER_DELEGATE_H_
