// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/error_test_util.h"

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "ios/web/public/web_state.h"
#import "ios/web/web_view/error_translation_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace testing {

NSError* CreateTestNetError(NSError* error) {
  return NetErrorFromError(error);
}

std::string GetErrorText(WebState* web_state,
                         const GURL& url,
                         const std::string& error_domain,
                         long error_code,
                         bool is_post,
                         bool is_off_the_record,
                         bool has_ssl_info) {
  return base::StringPrintf("web_state: %p url: %s domain: %s code: %ld post: "
                            "%d otr: %d ssl_info: %d",
                            web_state, url.spec().c_str(), error_domain.c_str(),
                            error_code, is_post, is_off_the_record,
                            has_ssl_info);
}

}  // namespace testing
}  // namespace web
