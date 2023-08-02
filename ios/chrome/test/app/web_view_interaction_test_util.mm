// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/web_view_interaction_test_util.h"

#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"

namespace chrome_test_util {

bool TapWebViewElementWithId(const std::string& element_id) {
  return web::test::TapWebViewElementWithId(GetCurrentWebState(), element_id);
}

bool TapWebViewElementWithIdInIframe(const std::string& element_id) {
  return web::test::TapWebViewElementWithIdInIframe(GetCurrentWebState(),
                                                    element_id);
}

bool TapWebViewElementWithId(const std::string& element_id,
                             NSError* __autoreleasing* error) {
  return web::test::TapWebViewElementWithId(GetCurrentWebState(), element_id,
                                            error);
}

void SubmitWebViewFormWithId(const std::string& form_id) {
  web::test::SubmitWebViewFormWithId(GetCurrentWebState(), form_id);
}

}  // namespace chrome_test_util
