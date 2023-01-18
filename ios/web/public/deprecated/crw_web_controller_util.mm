// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/deprecated/crw_web_controller_util.h"

#import <Foundation/Foundation.h>

#import "ios/web/web_state/ui/crw_swipe_recognizer_provider.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Gets the web controller from `web_state`.
CRWWebController* GetWebController(web::WebState* web_state) {
  return web::WebStateImpl::FromWebState(web_state)->GetWebController();
}
}  // namespace

namespace web_deprecated {

void SetSwipeRecognizerProvider(web::WebState* web_state,
                                id<CRWSwipeRecognizerProvider> delegate) {
  GetWebController(web_state).swipeRecognizerProvider = delegate;
}

}  // namespace web_deprecated
