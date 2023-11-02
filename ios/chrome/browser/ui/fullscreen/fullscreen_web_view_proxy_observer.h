// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_VIEW_PROXY_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_VIEW_PROXY_OBSERVER_H_

#import <Foundation/Foundation.h>

@protocol CRWWebViewProxy;
class FullscreenMediator;
class FullscreenModel;

// Helper object that observes the active WebState's CRWWebViewProxy.
@interface FullscreenWebViewProxyObserver : NSObject

// The proxy being observed.
@property(nonatomic, weak, nullable) id<CRWWebViewProxy> proxy;

// Designated initializer for an observer that uses `model` to update its proxy.
- (nullable instancetype)initWithModel:(nonnull FullscreenModel*)model
                              mediator:(nonnull FullscreenMediator*)mediator
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_WEB_VIEW_PROXY_OBSERVER_H_
