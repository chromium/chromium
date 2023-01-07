// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_SCROLL_VIEW_DELEGATE_PROXY_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_SCROLL_VIEW_DELEGATE_PROXY_H_

#import <UIKit/UIKit.h>

@class CRWWebViewScrollViewProxy;

// A delegate object of the UIScrollView managed by CRWWebViewScrollViewProxy.
//
// This class is separated from CRWWebViewScrollViewProxy mainly because both
// of CRWWebViewScrollViewProxy and CRWWebViewScrollViewDelegateProxy use
// -forwardInvocation: to forward unimplemented methods to different objects.
@interface CRWWebViewScrollViewDelegateProxy : NSObject <UIScrollViewDelegate>

- (nonnull instancetype)initWithScrollViewProxy:
    (nonnull CRWWebViewScrollViewProxy*)scrollViewProxy
    NS_DESIGNATED_INITIALIZER;

- (nonnull instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_SCROLL_VIEW_DELEGATE_PROXY_H_
