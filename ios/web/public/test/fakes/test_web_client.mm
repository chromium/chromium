// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_client.h"

#include "base/logging.h"
#include "ios/web/public/features.h"
#include "ios/web/test/test_url_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

TestWebClient::TestWebClient()
    : last_cert_error_code_(0), last_cert_error_overridable_(true) {}

TestWebClient::~TestWebClient() {}

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

void TestWebClient::SetEarlyPageScript(NSString* page_script) {
  early_page_script_ = [page_script copy];
}

void TestWebClient::AllowCertificateError(
    WebState* web_state,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
  last_cert_error_code_ = cert_error;
  last_cert_error_ssl_info_ = ssl_info;
  last_cert_error_request_url_ = request_url;
  last_cert_error_overridable_ = overridable;

  callback.Run(false);
}

}  // namespace web
