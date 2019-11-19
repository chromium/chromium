// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/shell/test/app/navigation_test_util.h"

#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace shell_test_util {

void LoadUrl(const GURL& url) {
  web::test::LoadUrl(GetCurrentWebState(), url);
}

bool IsLoading() {
  return GetCurrentWebState()->IsLoading();
}

}  // namespace shell_test_util
}  // namespace web
