// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_PRESENTER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_PRESENTER_DELEGATE_H_

#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"

@class PopupMenuPresenter;

// Protocol for an object which acts as a delegate for a popup menu presenter,
// and which is informed about dismissal events.
@protocol PopupMenuPresenterDelegate <ContainedPresenterDelegate>

// Tells the delegate that user took an action that will result in the dismissal
// of the presented view. It is the delegate's responsibility to call
// `dismissAnimated:`.
- (void)popupMenuPresenterWillDismiss:(PopupMenuPresenter*)presenter;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_PRESENTER_DELEGATE_H_
