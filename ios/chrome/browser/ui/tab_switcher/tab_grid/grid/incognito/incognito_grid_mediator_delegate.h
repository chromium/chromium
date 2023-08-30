// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_MEDIATOR_DELEGATE_H_

// Delegate allowing the incognito grid mediator to update the incognito tab
// grid coordinator.
@protocol IncognitoGridMediatorDelegate

// `disable` set to YES if the incognito view controller should be the disabled
// one.
- (void)shouldDisableIncognito:(BOOL)disable;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_MEDIATOR_DELEGATE_H_
