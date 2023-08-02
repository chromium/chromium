// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/app/web_view_interaction_test_util.h"

#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"

namespace web {
namespace shell_test_util {

void TapWebViewElementWithId(const std::string& element_id) {
  web::test::TapWebViewElementWithId(GetCurrentWebState(), element_id);
}

}  // namespace shell_test_util
}  // namespace web
