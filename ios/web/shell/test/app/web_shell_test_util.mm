// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/app/web_shell_test_util.h"

#import <UIKit/UIKit.h>

#import "ios/web/shell/view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace shell_test_util {

web::WebState* GetCurrentWebState() {
  ViewController* view_controller = static_cast<ViewController*>([[
      [[UIApplication sharedApplication] delegate] window] rootViewController]);
  return view_controller.webState;
}

}  // namespace shell_test_util
}  // namespace web
