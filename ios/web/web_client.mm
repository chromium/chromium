// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_client.h"

#import <Foundation/Foundation.h>

#import <string_view>

#import "ios/web/common/features.h"
#import "ios/web/public/init/web_main_parts.h"
#import "url/gurl.h"

namespace web {

static WebClient* g_client;

void SetWebClient(WebClient* client) {
  g_client = client;
}

WebClient* GetWebClient() {
  return g_client;
}

WebClient::Schemes::Schemes() = default;
WebClient::Schemes::~Schemes() = default;

WebClient::WebClient() {}

WebClient::~WebClient() {}

std::unique_ptr<WebMainParts> WebClient::CreateWebMainParts() {
  return nullptr;
}

std::string WebClient::GetApplicationLocale() const {
  return "en-US";
}

bool WebClient::IsAppSpecificURL(const GURL& url) const {
  return false;
}

std::string WebClient::GetUserAgent(UserAgentType type) const {
  return std::string();
}

std::u16string WebClient::GetLocalizedString(int message_id) const {
  return std::u16string();
}

std::string_view WebClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) const {
  return std::string_view();
}

base::RefCountedMemory* WebClient::GetDataResourceBytes(int resource_id) const {
  return nullptr;
}

std::vector<JavaScriptFeature*> WebClient::GetJavaScriptFeatures(
    BrowserState* browser_state) const {
  return std::vector<JavaScriptFeature*>();
}

void WebClient::PrepareErrorPage(WebState* web_state,
                                 const GURL& url,
                                 NSError* error,
                                 bool is_post,
                                 bool is_off_the_record,
                                 const std::optional<net::SSLInfo>& info,
                                 int64_t navigation_id,
                                 base::OnceCallback<void(NSString*)> callback) {
  DCHECK(error);
  std::move(callback).Run(error.localizedDescription);
}

UIView* WebClient::GetWindowedContainer() {
  return nullptr;
}

bool WebClient::EnableFullscreenAPI() const {
  return false;
}

bool WebClient::EnableLongPressUIContextMenu() const {
  return false;
}

bool WebClient::EnableWebInspector(BrowserState* browser_state) const {
  return false;
}

void WebClient::CleanupNativeRestoreURLs(web::WebState* web_state) const {}

void WebClient::WillDisplayMediaCapturePermissionPrompt(
    web::WebState* web_state) const {}

UserAgentType WebClient::GetDefaultUserAgent(web::WebState* web_state,
                                             const GURL& url) const {
  return UserAgentType::MOBILE;
}

void WebClient::LogDefaultUserAgent(web::WebState* web_state,
                                    const GURL& url) const {}

bool WebClient::IsPointingToSameDocument(const GURL& url1,
                                         const GURL& url2) const {
  return url1 == url2;
}

bool WebClient::IsBrowserLockdownModeEnabled() {
  return false;
}

void WebClient::SetOSLockdownModeEnabled(bool enabled) {}

bool WebClient::IsInsecureFormWarningEnabled(
    web::BrowserState* browser_state) const {
  return true;
}

}  // namespace web
