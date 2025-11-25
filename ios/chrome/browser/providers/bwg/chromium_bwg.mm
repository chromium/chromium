// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

namespace ios::provider {

// Script to check whether PageContext should be detached from the request.
constexpr const char16_t* kShouldDetachPageContextScript = u"return false;";

void StartBwgOverlay(BWGConfiguration* bwg_configuration) {}

const std::u16string GetPageContextShouldDetachScript() {
  return kShouldDetachPageContextScript;
}

id<BWGGatewayProtocol> CreateBWGGateway() {
  return nil;
}

void CheckGeminiEligibility(AuthenticationService* auth_service,
                            BWGEligibilityCallback completion) {}

void ResetGemini() {}

void UpdatePageAttachmentState(
    BWGPageContextAttachmentState bwg_attachment_state) {}

bool IsProtectedUrl(std::string url) {
  return false;
}

void UpdatePageContext(GeminiPageContext* gemini_page_context) {}

}  // namespace ios::provider
