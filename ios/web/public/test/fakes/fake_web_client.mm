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

void FakeWebClient::SetPluginNotSupportedText(const std::u16string& text) {
  plugin_not_supported_text_ = text;
}

void FakeWebClient::SetJavaScriptFeatures(
    std::vector<JavaScriptFeature*> features) {
  java_script_features_ = features;
}

void FakeWebClient::PrepareErrorPage(
    WebState* web_state,
    const GURL& url,
    NSError* error,
    bool is_post,
    bool is_off_the_record,
    const std::optional<net::SSLInfo>& info,
    int64_t navigation_id,
    base::OnceCallback<void(NSString*)> callback) {
  net::CertStatus cert_status = info.has_value() ? info.value().cert_status : 0;
  std::move(callback).Run(base::SysUTF8ToNSString(testing::GetErrorText(
      web_state, url, error, is_post, is_off_the_record, cert_status)));
}

UIView* FakeWebClient::GetWindowedContainer() {
  return GetAnyKeyWindow().rootViewController.view;
}

bool FakeWebClient::EnableWebInspector(web::BrowserState* browser_state) const {
  return true;
}

UserAgentType FakeWebClient::GetDefaultUserAgent(web::WebState* web_state,
                                                 const GURL& url) const {
  return default_user_agent_;
}

}  // namespace web
