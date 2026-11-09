// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_api.h"

#import <string>

#import "ios/chrome/test/providers/intelligence/proto_wrappers/test_page_context_wrapper_api.h"
#import "url/gurl.h"

namespace {

// Test detach script snippet controlled via `window.__gCrWeb.pageContext`.
constexpr char16_t kTestShouldDetachPageContextScript[] = uR"JS(
  if (window.__gCrWeb && window.__gCrWeb.pageContext) {
    if (typeof window.__gCrWeb.pageContext.shouldDetach === 'boolean') {
      return window.__gCrWeb.pageContext.shouldDetach;
    }
    if (window.__gCrWeb.pageContext.shouldTimeout) {
      while (true) {}
    }
  }
  return false;
)JS";

// Value returned by `IsProtectedUrl()`, overridden via
// `SetProtectedUrlForTesting()`.
bool g_is_protected_url_for_testing = false;

}  // namespace

namespace ios::provider {

namespace test {

void SetProtectedUrlForTesting(bool is_protected) {
  g_is_protected_url_for_testing = is_protected;
}

}  // namespace test

bool IsProtectedUrl(const GURL& url) {
  return g_is_protected_url_for_testing;
}

std::u16string GetPageContextShouldDetachScript() {
  return kTestShouldDetachPageContextScript;
}

}  // namespace ios::provider
