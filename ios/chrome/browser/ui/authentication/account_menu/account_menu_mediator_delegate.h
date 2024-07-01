// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol AccountMenuMediator;

@protocol AccountMenuMediatorDelegate <NSObject>

- (void)mediatorWantsToBeDismissed:(AccountMenuMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_MEDIATOR_DELEGATE_H_
