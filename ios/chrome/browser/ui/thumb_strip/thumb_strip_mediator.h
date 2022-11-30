// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_MEDIATOR_H_

#include <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"

@protocol CRWWebViewScrollViewProxyObserver;
class WebStateList;
class OverlayPresentationContext;

// Protocol for the thumb strip mediator to inform others about navigation
// changes.
@protocol ThumbStripNavigationConsumer
- (void)navigationDidStart;
@end

// Mediator for the thumb strip. Handles observing changes in the active web
// state.
@interface ThumbStripMediator : NSObject <ViewRevealingAnimatee>

// Consumer for this mediator to inform about updates.
@property(nonatomic, weak) id<ThumbStripNavigationConsumer> consumer;

// The regular web state list to observe.
@property(nonatomic, assign) WebStateList* regularWebStateList;
// The incognito web state list to observe.
@property(nonatomic, assign) WebStateList* incognitoWebStateList;

// The observer to register/deregister as CRWWebViewScrollViewProxyObserver for
// the active webstates in the given WebStateLists.
@property(nonatomic, weak) id<CRWWebViewScrollViewProxyObserver>
    webViewScrollViewObserver;

// The regular mode OverlayPresenter to notify when the thumb strip reveals or
// hides the associated view.
@property(nonatomic, assign)
    OverlayPresentationContext* regularOverlayPresentationContext;
// The incognito mode OverlayPresenter to notify when the thumb strip reveals or
// hides the associated view.
@property(nonatomic, assign)
    OverlayPresentationContext* incognitoOverlayPresentationContext;

@end

#endif  // IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_MEDIATOR_H_
