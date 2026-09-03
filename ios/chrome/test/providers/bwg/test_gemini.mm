// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import <optional>

#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"

@interface FakeBWGGateway : NSObject <BWGGatewayProtocol>
@end

@implementation FakeBWGGateway
@synthesize actuationHandler = _actuationHandler;
@synthesize cameraHandler = _cameraHandler;
@synthesize consentProviderHandler = _consentProviderHandler;
@synthesize pageStateChangeHandler = _pageStateChangeHandler;
@synthesize sessionHandler = _sessionHandler;
@synthesize suggestionHandler = _suggestionHandler;
@synthesize tabPickerHandler = _tabPickerHandler;
@end

namespace ios::provider {

void ConfigureWithStartupConfiguration(
    GeminiStartupConfiguration* startup_configuration) {}

void StartGeminiOverlay(GeminiConfiguration* gemini_configuration) {}

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

id<BWGGatewayProtocol> CreateGeminiGateway() {
  return [[FakeBWGGateway alloc] init];
}

void CheckGeminiEligibility(AuthenticationService* auth_service,
                            GeminiEligibilityCallback completion) {}

static GeminiViewState g_current_view_state = GeminiViewState::kUnknown;
static GeminiViewMode g_current_mode = GeminiViewMode::kUnknown;
static std::optional<gemini::EntryPoint>
    g_last_update_prompt_action_entry_point;
static NSString* g_last_update_prompt_action_prompt = nil;
static BOOL g_last_update_prompt_action_should_auto_submit = NO;

static bool g_mock_feature_mode_disabled_by_quota = false;
static NSDate* g_mock_refill_date = nil;

void ResetGemini() {
  g_current_mode = GeminiViewMode::kUnknown;
  g_current_view_state = GeminiViewState::kUnknown;
  g_last_update_prompt_action_entry_point.reset();
  g_last_update_prompt_action_prompt = nil;
  g_last_update_prompt_action_should_auto_submit = NO;
  g_mock_feature_mode_disabled_by_quota = false;
  g_mock_refill_date = nil;
}

void UpdatePageAttachmentState(
    GeminiPageContextAttachmentState gemini_attachment_state) {}

// Mock value used by unit tests to override the return value of IsProtectedUrl.
static bool g_mock_protected_url = false;

// Sets whether all URLs should be simulated as protected in tests.
void SetMockProtectedUrl(bool is_protected) {
  g_mock_protected_url = is_protected;
}

// Stub implementation for tests. Returns the mock value set by
// SetMockProtectedUrl.
bool IsProtectedUrl(std::string url) {
  return g_mock_protected_url;
}

void UpdateActivePageContext(GeminiPageContext* gemini_page_context,
                             NSArray<GeminiPageContext*>* shared_tabs) {}

NSArray<GeminiSettingsMetadata*>* GetEligibleSettings(
    AuthenticationService* auth_service) {
  return nil;
}

GeminiSettingsAction* ActionForSettingsContext(GeminiSettingsContext context) {
  return nil;
}

void UpdateOverlayOffsetWithOpacity(CGFloat offset, CGFloat opacity) {}

void UpdateDetentHeights(CGFloat collapsed_height, CGFloat extended_height) {}

void UpdateGeminiViewState(GeminiViewState view_state) {
  g_current_view_state = view_state;
}

void UpdateGeminiViewState(GeminiViewState view_state, bool animated) {
  g_current_view_state = view_state;
}

void UpdatePromptAction(gemini::EntryPoint entry_point,
                        NSString* prepopulated_prompt,
                        bool should_auto_submit) {
  g_last_update_prompt_action_entry_point = entry_point;
  g_last_update_prompt_action_prompt = [prepopulated_prompt copy];
  g_last_update_prompt_action_should_auto_submit = should_auto_submit;
}

std::optional<gemini::EntryPoint> GetLastUpdatePromptActionEntryPoint() {
  return g_last_update_prompt_action_entry_point;
}

NSString* GetLastUpdatePromptActionPrompt() {
  return g_last_update_prompt_action_prompt;
}

BOOL GetLastUpdatePromptActionShouldAutoSubmit() {
  return g_last_update_prompt_action_should_auto_submit;
}

GeminiViewState GetCurrentGeminiViewState() {
  return g_current_view_state;
}

void RequestUIChange(GeminiUIElementType ui_element_type) {}

void AttachImage(UIImage* image) {}

GeminiClientMode GetCurrentClientMode() {
  return GeminiClientMode::kUnknown;
}

GeminiPageContextAttachmentState GetCurrentPageContextAttachmentState() {
  return GeminiPageContextAttachmentState::kUnknown;
}

void SwitchToMode(GeminiViewMode mode, bool animated) {
  g_current_mode = mode;
}

void SwitchToMode(GeminiViewMode mode,
                  GeminiViewState target_state,
                  bool animated) {
  g_current_mode = mode;
  if (target_state != GeminiViewState::kUnknown) {
    g_current_view_state = target_state;
  }
}

GeminiViewMode GetCurrentMode() {
  return g_current_mode;
}

void SetLiveStopButtonHidden(bool hidden) {}

bool IsLiveStopButtonHidden() {
  return false;
}

void SetLiveCaptionsNumberOfLines(int number_of_lines) {}

int GetLiveCaptionsNumberOfLines() {
  return 0;
}

void SetShouldShowSuggestionChips(bool should_show) {}

void SetBlockQuerySubmissionWhileLoading(bool block_submission) {}

void SetShowPageLoadingSnackbarOnOpeningInvocation(bool show_snackbar) {}

void ShowAccountSnackbar() {}

UIViewController* GetFloatyViewControllerWithConfiguration(
    GeminiConfiguration* gemini_configuration) {
  UIViewController* viewController = [[UIViewController alloc] init];
  UITextField* textField = [[UITextField alloc] init];
  textField.accessibilityIdentifier = @"GeminiTestTextField";
  [viewController.view addSubview:textField];
  return viewController;
}

void SetMockFeatureModeDisabledByQuota(bool disabled) {
  g_mock_feature_mode_disabled_by_quota = disabled;
}

void SetMockRefillDateForFeatureMode(NSDate* date) {
  g_mock_refill_date = date;
}

bool IsFeatureModeDisabledByQuota(GeminiFeatureMode feature_mode) {
  return g_mock_feature_mode_disabled_by_quota;
}

NSDate* GetRefillDateForFeatureMode(GeminiFeatureMode feature_mode) {
  return g_mock_refill_date;
}

}  // namespace ios::provider
