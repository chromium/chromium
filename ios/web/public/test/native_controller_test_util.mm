// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/native_controller_test_util.h"

#import "ios/web/public/deprecated/crw_native_content_holder.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace test {

id<CRWNativeContent> GetCurrentNativeController(WebState* web_state) {
  web::WebStateImpl* web_state_impl =
      static_cast<web::WebStateImpl*>(web_state);
  return [[web_state_impl->GetWebController() nativeContentHolder]
      nativeController];
}

}  // namespace test
}  // namespace web
