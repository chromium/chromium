// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/open_in/open_in_tab_helper_delegate.h"

class WebStateList;

// Mediator which mediates between openIn views and openIn tab helpers.
@interface OpenInMediator : NSObject <OpenInTabHelperDelegate>

- (instancetype)initWithWebStateList:(WebStateList*)webStateList;
// Disables all registered openInControllers.
- (void)disableAll;

@end

#endif  // IOS_CHROME_BROWSER_UI_OPEN_IN_OPEN_IN_MEDIATOR_H_
