// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/app/navigation_test_util.h"

#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"

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
