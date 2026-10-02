// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/suppress_input_views_app_interface.h"

#import "base/check.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"

@implementation SuppressInputViewsAppInterface

+ (void)setShouldSuppressInputViews:(BOOL)shouldSuppressInputViews {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  CHECK(webState);
  webState->GetWebViewProxy().shouldSuppressInputViews =
      shouldSuppressInputViews;
}

@end
