// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"

namespace ios::provider {

// Script to check whether PageContext should be detached from the request.
constexpr const char16_t* kShouldDetachPageContextScript = u"return false;";

void ConfigureWithStartupConfiguration(
    GeminiStartupConfiguration* gemini_startup_configuration) {}

// TODO(crbug.com/478259873): Replace with StartGeminiOverlay
void StartBwgOverlay(GeminiConfiguration* gemini_configuration) {}

const std::u16string GetPageContextShouldDetachScript() {
  return kShouldDetachPageContextScript;
}

// TODO(crbug.com/478259873): Replace with CreateGeminiGateway
id<BWGGatewayProtocol> CreateBWGGateway() {
  return nil;
}

void CheckGeminiEligibility(AuthenticationService* auth_service,
                            GeminiEligibilityCallback completion) {}

void ResetGemini() {}

void UpdatePageAttachmentState(
    GeminiPageContextAttachmentState gemini_attachment_state) {}

void UpdatePromptAction(gemini::EntryPoint entry_point,
                        NSString* prepopulated_prompt) {}

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

void UpdateOverlayOffsetWithOpacity(CGFloat offset, CGFloat opacity) {}

void UpdateGeminiViewState(GeminiViewState view_state, bool animated) {}

GeminiViewState GetCurrentGeminiViewState() {
  return GeminiViewState::kUnknown;
}

void RequestUIChange(GeminiUIElementType ui_element_type) {}

void AttachImage(UIImage* image) {}

GeminiClientMode GetCurrentClientMode() {
  return GeminiClientMode::kUnknown;
}

GeminiPageContextAttachmentState GetCurrentPageContextAttachmentState() {
  return GeminiPageContextAttachmentState::kUnknown;
}

void SwitchToMode(GeminiViewMode mode, bool animated) {}

GeminiViewMode GetCurrentMode() {
  return GeminiViewMode::kUnknown;
}

void SetLiveStopButtonHidden(bool hidden) {}

bool IsLiveStopButtonHidden() {
  return false;
}

void SetLiveCaptionsNumberOfLines(int number_of_lines) {}

int GetLiveCaptionsNumberOfLines() {
  return 0;
}

UIViewController* GetFloatyViewControllerWithConfiguration(
    GeminiConfiguration* gemini_configuration) {
  return nil;
}

}  // namespace ios::provider
