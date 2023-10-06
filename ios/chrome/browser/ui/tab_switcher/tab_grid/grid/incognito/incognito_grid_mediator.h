// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_MEDIATOR_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"

@protocol IncognitoGridMediatorDelegate;
@protocol TabCollectionConsumer;
class PrefService;

// Mediates between model layer and incognito grid UI layer.
@interface IncognitoGridMediator : BaseGridMediator

// Incognito mediator delegate.
@property(nonatomic, weak) id<IncognitoGridMediatorDelegate> incognitoDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_MEDIATOR_H_
