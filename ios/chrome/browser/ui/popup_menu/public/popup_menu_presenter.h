// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_PRESENTER_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_PRESENTER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter_delegate.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter.h"

// Presenter for the popup menu. It handles showing/dismissing a popup menu.
@interface PopupMenuPresenter : NSObject <ContainedPresenter>

// The delegate object which will be told about presentation events. Overrides
// parent class property.
@property(nonatomic, weak) id<PopupMenuPresenterDelegate> delegate;

// Layout guide used for the presentation.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_PUBLIC_POPUP_MENU_PRESENTER_H_
