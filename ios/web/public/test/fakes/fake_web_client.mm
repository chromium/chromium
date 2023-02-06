// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_web_client.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/crw_fake_find_session.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/test/test_url_constants.h"
#import "ui/base/resource/resource_bundle.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FakeWebClient::FakeWebClient() = default;

FakeWebClient::~FakeWebClient() = default;

void FakeWebClient::AddAdditionalSchemes(Schemes* schemes) const {
  schemes->standard_schemes.push_back(kTestWebUIScheme);
  schemes->standard_schemes.push_back(kTestAppSpecificScheme);
}

bool FakeWebClient::IsAppSpecificURL(const GURL& url) const {
  return url.SchemeIs(kTestWebUIScheme) || url.SchemeIs(kTestAppSpecificScheme);
}

std::u16string FakeWebClient::GetPluginNotSupportedText() const {
  return plugin_not_supported_text_;
}

std::string FakeWebClient::GetUserAgent(UserAgentType type) const {
  if (type == UserAgentType::DESKTOP)
    return "Chromium/66.0.3333.0 CFNetwork/893.14 Darwin/16.7.0 Desktop";
  return "Chromium/66.0.3333.0 CFNetwork/893.14 Darwin/16.7.0 Mobile";
}

base::RefCountedMemory* FakeWebClient::GetDataResourceBytes(
    int resource_id) const {
  if (!ui::ResourceBundle::HasSharedInstance())
    return nullptr;
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::vector<JavaScriptFeature*> FakeWebClient::GetJavaScriptFeatures(
    BrowserState* browser_state) const {
  return java_script_features_;
}

NSString* FakeWebClient::GetDocumentStartScriptForMainFrame(
    BrowserState* browser_state) const {
  return early_page_script_ ? early_page_script_ : @"";
}

void FakeWebClient::SetPluginNotSupportedText(const std::u16string& text) {
  plugin_not_supported_text_ = text;
}

void FakeWebClient::SetJavaScriptFeatures(
    std::vector<JavaScriptFeature*> features) {
  java_script_features_ = features;
}

void FakeWebClient::SetEarlyPageScript(NSString* page_script) {
  early_page_script_ = [page_script copy];
}

void FakeWebClient::PrepareErrorPage(
    WebState* web_state,
    const GURL& url,
    NSError* error,
    bool is_post,
    bool is_off_the_record,
    const absl::optional<net::SSLInfo>& info,
    int64_t navigation_id,
    base::OnceCallback<void(NSString*)> callback) {
  net::CertStatus cert_status = info.has_value() ? info.value().cert_status : 0;
  std::move(callback).Run(base::SysUTF8ToNSString(testing::GetErrorText(
      web_state, url, error, is_post, is_off_the_record, cert_status)));
}

UIView* FakeWebClient::GetWindowedContainer() {
  return GetAnyKeyWindow().rootViewController.view;
}

UserAgentType FakeWebClient::GetDefaultUserAgent(web::WebState* web_state,
                                                 const GURL& url) const {
  return default_user_agent_;
}

void FakeWebClient::SetFindSessionPrototype(
    CRWFakeFindSession* find_session_prototype) API_AVAILABLE(ios(16)) {
  find_session_prototype_ = find_session_prototype;
}

id<CRWFindSession> FakeWebClient::CreateFindSessionForWebState(
    web::WebState* web_state) const API_AVAILABLE(ios(16)) {
  return find_session_prototype_ ? [find_session_prototype_ copy]
                                 : [[CRWFakeFindSession alloc] init];
}

void FakeWebClient::StartTextSearchInWebState(web::WebState* web_state) {
  text_search_started_ = true;
}

void FakeWebClient::StopTextSearchInWebState(web::WebState* web_state) {
  text_search_started_ = false;
}

bool FakeWebClient::IsTextSearchStarted() const {
  return text_search_started_;
}

}  // namespace web
