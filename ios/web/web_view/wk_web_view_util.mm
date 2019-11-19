// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_view/wk_web_view_util.h"

#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

bool IsSafeBrowsingWarningDisplayedInWebView(WKWebView* web_view) {
  // A SafeBrowsing warning is a UIScrollView that is inserted on top of
  // WKWebView's scroll view. This method uses heuristics to detect this view.
  // It may break in the future if WebKit's implementation of SafeBrowsing
  // warnings changes.
  UIView* containing_view = web_view.scrollView.superview;
  if (!containing_view)
    return false;

  UIView* top_view = containing_view.subviews.lastObject;

  if (top_view == web_view.scrollView)
    return false;

  return [top_view isKindOfClass:[UIScrollView class]] &&
         [NSStringFromClass([top_view class]) containsString:@"Warning"] &&
         top_view.subviews.count > 0 &&
         [top_view.subviews.firstObject.subviews.lastObject
             isKindOfClass:[UIButton class]];
}

bool RequiresContentFilterBlockingWorkaround() {
  if (!GetWebClient()->IsSlimNavigationManagerEnabled())
    return false;

  // This is fixed in iOS13 beta 7.
  if (@available(iOS 13, *))
    return false;

  if (@available(iOS 12.2, *))
    return true;

  return false;
}

bool RequiresProvisionalNavigationFailureWorkaround() {
  if (@available(iOS 12.2, *))
    return true;
  return false;
}
}  // namespace web
