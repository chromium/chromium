// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRAG_AND_DROP_DROP_AND_NAVIGATE_INTERACTION_H_
#define IOS_CHROME_BROWSER_DRAG_AND_DROP_DROP_AND_NAVIGATE_INTERACTION_H_

#import <UIKit/UIKit.h>

@protocol DropAndNavigateDelegate;

// A UIDropManager that notifies it's DropAndNavigateDelegate whenever an object
// that can trigger a navigation is dropped.
API_AVAILABLE(ios(11.0))
@interface DropAndNavigateInteraction : UIDropInteraction

// Default initializer. Notifies |navigationDelegate| whenever an item
// that can trigger a navigation was dropped. |navigationDelegate| is not
// retained.
- (instancetype)initWithDelegate:(id<DropAndNavigateDelegate>)navigationDelegate
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_DRAG_AND_DROP_DROP_AND_NAVIGATE_INTERACTION_H_
