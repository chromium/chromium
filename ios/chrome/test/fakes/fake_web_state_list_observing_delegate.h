// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_WEB_STATE_LIST_OBSERVING_DELEGATE_H_
#define IOS_CHROME_TEST_FAKES_FAKE_WEB_STATE_LIST_OBSERVING_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

class WebStateList;

// Defines a WebStateListObserving for use in unittests. Used to test if an
// observer method was called or not.
@interface FakeWebStateListObservingDelegate : NSObject <WebStateListObserving>
@property(nonatomic, assign) BOOL didMoveWebStateWasCalled;
- (void)observeWebStateList:(WebStateList*)webStateList;
- (void)stopObservingWebStateList:(WebStateList*)webStateList;

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_WEB_STATE_LIST_OBSERVING_DELEGATE_H_
