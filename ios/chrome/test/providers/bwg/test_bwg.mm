// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"

namespace ios::provider {

void StartBwgOverlay(GeminiConfiguration* gemini_configuration) {}

const std::u16string GetPageContextShouldDetachScript() {
  return uR"JS(
      if (window.__gCrWeb && window.__gCrWeb.pageContext) {
        if (typeof window.__gCrWeb.pageContext.shouldDetach === 'boolean') {
          return window.__gCrWeb.pageContext.shouldDetach;
        }
        if (window.__gCrWeb.pageContext.shouldTimeout) {
          while(true);
        }
      }
      return false;
  )JS";
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

NSArray<GeminiSettingsMetadata*>* GetEligibleSettings(
    AuthenticationService* auth_service) {
  return nil;
}

GeminiSettingsAction* ActionForSettingsContext(GeminiSettingsContext context) {
  return nil;
}

void UpdateOverlayOffset(CGFloat offset) {}

void UpdateGeminiViewState(GeminiViewState view_state) {}

}  // namespace ios::provider
