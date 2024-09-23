// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "url/gurl.h"

BOOL IsSwipingBack(UISwipeGestureRecognizerDirection direction) {
  if (UseRTLLayout())
    return direction == UISwipeGestureRecognizerDirectionLeft;
  else
    return direction == UISwipeGestureRecognizerDirectionRight;
}

BOOL IsSwipingForward(UISwipeGestureRecognizerDirection direction) {
  if (UseRTLLayout())
    return direction == UISwipeGestureRecognizerDirectionRight;
  else
    return direction == UISwipeGestureRecognizerDirectionLeft;
}

BOOL UseNativeSwipe(web::NavigationItem* item) {
  if (!item)
    return NO;

  if (IsURLNewTabPage(item->GetVirtualURL()))
    return YES;

  GURL url(item->GetURL());
  if (UrlHasChromeScheme(url) && url.host_piece() == kChromeUICrashHost)
    return YES;

  return NO;
}
