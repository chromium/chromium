// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

namespace ios::provider {

// Script to check whether PageContext should be detached from the request, for
// testing.
const std::u16string kShouldDetachPageContextScriptForTesting =
    u"return false;";

std::string CreateRequestBody(
    std::string prompt,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context) {
  return std::string();
}

std::unique_ptr<network::ResourceRequest> CreateResourceRequest() {
  return nullptr;
}

void StartBwgOverlay(
    UIViewController* base_view_controller,
    raw_ptr<AuthenticationService> auth_service,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context) {}

const std::u16string GetPageContextShouldDetachScript() {
  return kShouldDetachPageContextScriptForTesting;
}

}  // namespace ios::provider
