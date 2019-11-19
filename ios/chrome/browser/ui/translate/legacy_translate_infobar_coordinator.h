// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TRANSLATE_LEGACY_TRANSLATE_INFOBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TRANSLATE_LEGACY_TRANSLATE_INFOBAR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// The a11y identifier for the translate infobar's language selection menu.
extern NSString* const kLanguageSelectorPopupMenuId;
// The a11y identifier for the translate infobar's translate options menu.
extern NSString* const kTranslateOptionsPopupMenuId;

@protocol SnackbarCommands;
class WebStateList;

// Coordinator responsible for presenting and dismissing the translate infobar's
// language selection popup menu, translate options popup menu, and translate
// options notifications.
@interface LegacyTranslateInfobarCoordinator : ChromeCoordinator

// Creates a coordinator that uses |viewController|, |browserState|, and
// |webStateList|.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList
                                dispatcher:(id<SnackbarCommands>)dispatcher;

// Unavailable, use -initWithBaseViewController:browserState:webStateList:.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TRANSLATE_LEGACY_TRANSLATE_INFOBAR_COORDINATOR_H_
