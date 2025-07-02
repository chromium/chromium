// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_browser_agent.h"

#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_link_opening_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_gateway_protocol.h"

namespace {

// Helper to convert PageContextWrapperError to BWGPageContextState.
ios::provider::BWGPageContextState BWGPageContextFromPageContextWrapperError(
    PageContextWrapperError error) {
  switch (error) {
    case PageContextWrapperError::kForceDetachError:
      return ios::provider::BWGPageContextState::kProtected;
    default:
      return ios::provider::BWGPageContextState::kError;
  }
}

}  // namespace

BwgBrowserAgent::BwgBrowserAgent(Browser* browser) : BrowserUserData(browser) {
  bwg_gateway_ = ios::provider::CreateBWGGateway();

  if (bwg_gateway_) {
    bwg_link_opening_handler_ = [[BWGLinkOpeningHandler alloc] init];
    bwg_gateway_.linkOpeningHandler = bwg_link_opening_handler_;

    bwg_session_handler_ = [[BWGSessionHandler alloc]
        initWithWebStateList:browser_->GetWebStateList()];
    bwg_gateway_.sessionHandler = bwg_session_handler_;
  }
}

BwgBrowserAgent::~BwgBrowserAgent() = default;

void BwgBrowserAgent::PresentBwgOverlay(
    UIViewController* base_view_controller,
    base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                   PageContextWrapperError> expected_page_context) {
  BWGConfiguration* config = [[BWGConfiguration alloc] init];
  config.baseViewController = base_view_controller;
  config.authService =
      AuthenticationServiceFactory::GetForProfile(browser_->GetProfile());
  config.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();
  config.gateway = bwg_gateway_;

  BwgTabHelper* bwg_tab_helper = BwgTabHelper::FromWebState(
      browser_->GetWebStateList()->GetActiveWebState());
  config.clientID = base::SysUTF8ToNSString(bwg_tab_helper->GetClientId());
  std::optional<std::string> maybe_server_id = bwg_tab_helper->GetServerId();
  config.serverID =
      maybe_server_id ? base::SysUTF8ToNSString(*maybe_server_id) : nil;

  std::unique_ptr<optimization_guide::proto::PageContext> pageContext = nullptr;
  if (expected_page_context.has_value()) {
    pageContext = std::move(expected_page_context.value());
    config.BWGPageContextState =
        ios::provider::BWGPageContextState::kSuccessfullyAttached;
  } else {
    config.BWGPageContextState = BWGPageContextFromPageContextWrapperError(
        expected_page_context.error());
  }
  config.uniquePageContext = std::move(pageContext);

  ios::provider::StartBwgOverlay(config);
  bwg_tab_helper->SetBwgSessionActive(true);
}
