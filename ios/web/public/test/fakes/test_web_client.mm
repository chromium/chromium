// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_client.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "ios/web/public/test/error_test_util.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/test/test_url_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

TestWebClient::TestWebClient() = default;

TestWebClient::~TestWebClient() = default;

void TestWebClient::AddAdditionalSchemes(Schemes* schemes) const {
  schemes->standard_schemes.push_back(kTestWebUIScheme);
  schemes->standard_schemes.push_back(kTestNativeContentScheme);
  schemes->standard_schemes.push_back(kTestAppSpecificScheme);
}

bool TestWebClient::IsAppSpecificURL(const GURL& url) const {
  return url.SchemeIs(kTestWebUIScheme) ||
         url.SchemeIs(kTestNativeContentScheme) ||
         url.SchemeIs(kTestAppSpecificScheme);
}

bool TestWebClient::ShouldBlockUrlDuringRestore(const GURL& url,
                                                WebState* web_state) const {
  return false;
}

void TestWebClient::AddSerializableData(
    web::SerializableUserDataManager* user_data_manager,
    web::WebState* web_state) {}

base::string16 TestWebClient::GetPluginNotSupportedText() const {
  return plugin_not_supported_text_;
}

std::string TestWebClient::GetUserAgent(UserAgentType type) const {
  return "Chromium/66.0.3333.0 CFNetwork/893.14 Darwin/16.7.0";
}

base::RefCountedMemory* TestWebClient::GetDataResourceBytes(
    int resource_id) const {
  if (!ui::ResourceBundle::HasSharedInstance())
    return nullptr;
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

NSString* TestWebClient::GetDocumentStartScriptForMainFrame(
    BrowserState* browser_state) const {
  return early_page_script_ ? early_page_script_ : @"";
}

void TestWebClient::SetPluginNotSupportedText(const base::string16& text) {
  plugin_not_supported_text_ = text;
}

void TestWebClient::SetEarlyPageScript(NSString* page_script) {
  early_page_script_ = [page_script copy];
}

void TestWebClient::AllowCertificateError(
    WebState* web_state,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    int64_t navigation_id,
    const base::Callback<void(bool)>& callback) {
  last_cert_error_code_ = cert_error;
  last_cert_error_ssl_info_ = ssl_info;
  last_cert_error_request_url_ = request_url;
  last_cert_error_overridable_ = overridable;

  // Embedder should consult the user, so reply is asynchronous.
  base::PostTask(FROM_HERE, {WebThread::UI},
                 base::BindOnce(callback, allow_certificate_errors_));
}

void TestWebClient::SetAllowCertificateErrors(bool flag) {
  allow_certificate_errors_ = flag;
}

void TestWebClient::PrepareErrorPage(
    WebState* web_state,
    const GURL& url,
    NSError* error,
    bool is_post,
    bool is_off_the_record,
    const base::Optional<net::SSLInfo>& info,
    int64_t navigation_id,
    base::OnceCallback<void(NSString*)> callback) {
  std::move(callback).Run(base::SysUTF8ToNSString(testing::GetErrorText(
      web_state, url, base::SysNSStringToUTF8(error.domain), error.code,
      is_post, is_off_the_record, info.has_value())));
}

UIView* TestWebClient::GetWindowedContainer() {
  return UIApplication.sharedApplication.keyWindow.rootViewController.view;
}

}  // namespace web
