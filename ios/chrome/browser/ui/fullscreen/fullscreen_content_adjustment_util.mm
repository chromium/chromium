// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_content_adjustment_util.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/web/common/features.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"

void MoveContentBelowHeader(id<CRWWebViewProxy> proxy, FullscreenModel* model) {
  DCHECK(proxy);
  DCHECK(model);
  CGFloat topPadding = model->current_toolbar_insets().top;
  proxy.scrollViewProxy.contentOffset = CGPointMake(0, -topPadding);
}
