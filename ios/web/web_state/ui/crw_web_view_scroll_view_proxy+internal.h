// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_SCROLL_VIEW_PROXY_INTERNAL_H_
#define IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_SCROLL_VIEW_PROXY_INTERNAL_H_

#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

@class CRBProtocolObservers;

// Declares internal API for this class. This API should only be used in
// //ios/web.
@interface CRWWebViewScrollViewProxy (Internal)

// Observers of this proxy which subscribe to change notifications.
@property(nonatomic, readonly)
    CRBProtocolObservers<CRWWebViewScrollViewProxyObserver>* observers;

// The underlying UIScrollView. It can change.
//
// The property supports assigning nil, but it returns a placeholder scroll view
// instead of nil in that case.
//
// This must be a strong reference to:
//   - avoid situation when the underlying scroll view is deallocated while
//     associated with the proxy, which would prevent the proxy to preserve its
//     properties
//   - retain the placeholder scroll view
@property(nonatomic, readonly) UIScrollView* underlyingScrollView;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_WEB_VIEW_SCROLL_VIEW_PROXY_INTERNAL_H_
