// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_api.h"

#import <string>

#import "url/gurl.h"

namespace {

// Default script body when no internal detachment predicate is configured.
constexpr char16_t kShouldDetachPageContextScript[] = u"return false;";

}  // namespace

namespace ios::provider {

bool IsProtectedUrl(const GURL& url) {
  return false;
}

std::u16string GetPageContextShouldDetachScript() {
  return kShouldDetachPageContextScript;
}

}  // namespace ios::provider
