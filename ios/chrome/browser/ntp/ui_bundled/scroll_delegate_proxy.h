// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_SCROLL_DELEGATE_PROXY_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_SCROLL_DELEGATE_PROXY_H_

#import <UIKit/UIKit.h>

// A proxy class that intercepts and forwards UIScrollViewDelegate callbacks
// to two targets: a primary intercepting target (e.g. the bottom sheet VC)
// and the original target (e.g. the feed controller's scroll delegate).
@interface ScrollDelegateProxy : NSProxy <UIScrollViewDelegate>

// The target that intercepts callbacks first.
@property(nonatomic, weak, readonly)
    NSObject<UIScrollViewDelegate>* interceptingTarget;

// The original target that also receives all callbacks.
@property(nonatomic, weak, readonly)
    NSObject<UIScrollViewDelegate>* originalTarget;

- (instancetype)initWithInterceptingTarget:
                    (NSObject<UIScrollViewDelegate>*)interceptingTarget
                            originalTarget:
                                (NSObject<UIScrollViewDelegate>*)originalTarget;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_SCROLL_DELEGATE_PROXY_H_
