// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_consumer.h"

@protocol ShortcutsViewControllerDelegate;

// The view controller displaying the omnibox shortcuts in the zero state.
@interface ShortcutsViewController
    : UICollectionViewController <ShortcutsConsumer>

@property(nonatomic, weak) id<ShortcutsViewControllerDelegate> commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_SHORTCUTS_SHORTCUTS_VIEW_CONTROLLER_H_
