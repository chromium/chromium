// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_PRIVATE_CONSTRUCTORS_H_
#define IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_PRIVATE_CONSTRUCTORS_H_

#include <memory>
#include <vector>

#import "ios/web/navigation/crw_session_controller.h"
#include "ios/web/public/deprecated/navigation_item_list.h"

namespace web {
class BrowserState;
class NavigationItem;
}

// Temporary interface for NavigationManager and tests to create
// CRWSessionControllers. Once CRWSessionController has no users outside of
// web/, these methods can go back into session_controller.h. crbug.com/318974
@interface CRWSessionController (PrivateConstructors)
// Initializes a session controller.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState;

// Initializes a session controller, supplying a list of NavigationItem objects
// and the last committed item index in the navigation history.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
                     navigationItems:(web::ScopedNavigationItemList)items
              lastCommittedItemIndex:(NSUInteger)lastCommittedItemIndex;
@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_PRIVATE_CONSTRUCTORS_H_
