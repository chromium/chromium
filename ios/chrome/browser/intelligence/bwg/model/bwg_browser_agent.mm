// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_browser_agent.h"

#import "base/strings/sys_string_conversions.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_page_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_suggestion_handler.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"
#import "ui/gfx/image/image.h"

namespace {

// Helper to convert PageContextWrapperError to BWGPageContextComputationState.
ios::provider::BWGPageContextComputationState
BWGPageContextComputationStateFromPageContextWrapperError(
    PageContextWrapperError error) {
  switch (error) {
    case PageContextWrapperError::kForceDetachError:
      return ios::provider::BWGPageContextComputationState::kProtected;
    default:
      return ios::provider::BWGPageContextComputationState::kError;
  }
}

}  // namespace

BwgBrowserAgent::BwgBrowserAgent(Browser* browser) : BrowserUserData(browser) {
  bwg_gateway_ = ios::provider::CreateBWGGateway();

  if (bwg_gateway_) {
    bwg_link_opening_handler_ = [[BWGLinkOpeningHandler alloc]
        initWithURLLoader:UrlLoadingBrowserAgent::FromBrowser(browser_)];
    bwg_gateway_.linkOpeningHandler = bwg_link_opening_handler_;

    bwg_page_state_change_handler_ = [[BWGPageStateChangeHandler alloc]
        initWithPrefService:browser_->GetProfile()->GetPrefs()];
    bwg_gateway_.pageStateChangeHandler = bwg_page_state_change_handler_;

    bwg_session_handler_ = [[BWGSessionHandler alloc]
        initWithWebStateList:browser_->GetWebStateList()];
    bwg_gateway_.sessionHandler = bwg_session_handler_;

    gemini_suggestion_handler_ = [[GeminiSuggestionHandler alloc]
        initWithWebStateList:browser_->GetWebStateList()];
    bwg_gateway_.suggestionHandler = gemini_suggestion_handler_;
  }
}

BwgBrowserAgent::~BwgBrowserAgent() = default;

void BwgBrowserAgent::PresentBwgOverlay(
    UIViewController* base_view_controller,
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError> expected_page_context) {
  if (expected_page_context.has_value()) {
    PresentBwgOverlayWithState(
        base_view_controller, std::move(expected_page_context.value()),
        ios::provider::BWGPageContextComputationState::kSuccess);
  } else {
    PresentBwgOverlayWithState(
        base_view_controller,
        /*page_context_proto=*/nullptr,
        BWGPageContextComputationStateFromPageContextWrapperError(
            expected_page_context.error()));
  }
}

void BwgBrowserAgent::PresentPendingBwgOverlay(
    UIViewController* base_view_controller,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context) {
  PresentBwgOverlayWithState(
      base_view_controller, std::move(page_context),
      ios::provider::BWGPageContextComputationState::kPending);
}

void BwgBrowserAgent::UpdateBwgOverlayPageContext(
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError> expected_page_context) {
  GeminiPageContext* gemini_page_context = [[GeminiPageContext alloc] init];
  gemini_page_context.BWGPageContextComputationState =
      ios::provider::BWGPageContextComputationState::kSuccess;

  std::unique_ptr<optimization_guide::proto::PageContext> page_context_proto =
      nullptr;
  if (expected_page_context.has_value()) {
    page_context_proto = std::move(expected_page_context.value());
  } else {
    gemini_page_context.BWGPageContextComputationState =
        BWGPageContextComputationStateFromPageContextWrapperError(
            expected_page_context.error());
  }
  gemini_page_context.uniquePageContext = std::move(page_context_proto);
  gemini_page_context.favicon = FetchPageFavicon();

  ApplyUserPrefsToPageContext(gemini_page_context);
  ios::provider::UpdatePageContext(gemini_page_context);
}

#pragma mark - Private

void BwgBrowserAgent::PresentBwgOverlayWithState(
    UIViewController* base_view_controller,
    std::unique_ptr<optimization_guide::proto::PageContext> page_context_proto,
    ios::provider::BWGPageContextComputationState computation_state) {
  SetSessionCommandHandlers();
  [bwg_page_state_change_handler_ setBaseViewController:base_view_controller];

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();

  BWGConfiguration* config = [[BWGConfiguration alloc] init];
  config.baseViewController = base_view_controller;
  config.authService =
      AuthenticationServiceFactory::GetForProfile(browser_->GetProfile());
  config.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  config.gateway = bwg_gateway_;

  // Use the tab helper to set the initial floaty state, which includes the chat
  // IDs and whether it was backgrounded.
  BwgTabHelper* bwg_tab_helper = BwgTabHelper::FromWebState(web_state);
  config.clientID = base::SysUTF8ToNSString(bwg_tab_helper->GetClientId());
  std::optional<std::string> maybe_server_id = bwg_tab_helper->GetServerId();
  config.serverID =
      maybe_server_id ? base::SysUTF8ToNSString(*maybe_server_id) : nil;
  config.shouldAnimatePresentation =
      !bwg_tab_helper->GetIsBwgSessionActiveInBackground();
  config.lastInteractionURLDifferent =
      bwg_tab_helper->IsLastInteractionUrlDifferent();
  config.shouldShowSuggestionChips =
      bwg_tab_helper->ShouldShowSuggestionChips();
  config.contextualCueChipLabel = bwg_tab_helper->GetContextualCueLabel();

  // Set the location permission state.
  // TODO(crbug.com/426207968): Populate with actual value.
  config.BWGLocationPermissionState =
      ios::provider::BWGLocationPermissionState::kUnknown;

  // Set the page context itself and page context computation/attachment state
  // for the current web state.
  config.pageContext = [[GeminiPageContext alloc] init];
  config.pageContext.BWGPageContextComputationState = computation_state;
  config.pageContext.uniquePageContext = std::move(page_context_proto);
  config.pageContext.favicon = FetchPageFavicon();
  ApplyUserPrefsToPageContext(config.pageContext);

  // Start the overlay and update the tab helper to reflect this.
  ios::provider::StartBwgOverlay(config);
  bwg_tab_helper->SetBwgUiShowing(true);
}

UIImage* BwgBrowserAgent::FetchPageFavicon() {
  // Use the cached favicon of the web state. If it's not available, use a
  // default favicon instead.
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  gfx::Image cached_favicon =
      favicon::WebFaviconDriver::FromWebState(web_state)->GetFavicon();
  if (!cached_favicon.IsEmpty()) {
    return cached_favicon.ToUIImage();
  }
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:gfx::kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];
  return DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
}

void BwgBrowserAgent::ApplyUserPrefsToPageContext(
    GeminiPageContext* gemini_page_context) {
  // Disable the page context attachment state based on user prefs.
  PrefService* pref_service = browser_->GetProfile()->GetPrefs();
  if (!pref_service->GetBoolean(prefs::kIOSBWGPageContentSetting)) {
    gemini_page_context.BWGPageContextAttachmentState =
        ios::provider::BWGPageContextAttachmentState::kUserDisabled;
  } else {
    // If page context is not disabled by the user, page context is always
    // available and should be attached. Note page context is only partially
    // available (e.g. title, url, favicon) while
    // `BWGPageContextComputationState` is pending.
    gemini_page_context.BWGPageContextAttachmentState =
        ios::provider::BWGPageContextAttachmentState::kAttached;
  }
}

void BwgBrowserAgent::SetSessionCommandHandlers() {
  id<SettingsCommands> settings_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), SettingsCommands);
  id<BWGCommands> bwg_handler =
      HandlerForProtocol(browser_->GetCommandDispatcher(), BWGCommands);

  bwg_session_handler_.settingsHandler = settings_handler;
  bwg_session_handler_.BWGHandler = bwg_handler;
}
