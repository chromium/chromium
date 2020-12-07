// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_MEDIATOR_H_

#include <UIKit/UIKit.h>

@protocol CRWWebViewScrollViewProxyObserver;
class WebStateList;

// Mediator for the thumb strip. Handles observing changes in the active web
// state.
@interface ThumbStripMediator : NSObject

// The regular web state list to observe.
@property(nonatomic, assign) WebStateList* regularWebStateList;
// The incognito web state list to observe.
@property(nonatomic, assign) WebStateList* incognitoWebStateList;

// The observer to register/deregister as CRWWebViewScrollViewProxyObserver for
// the active webstates in the given WebStateLists.
@property(nonatomic, weak) id<CRWWebViewScrollViewProxyObserver>
    webViewScrollViewObserver;

@end

#endif  // IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_MEDIATOR_H_
