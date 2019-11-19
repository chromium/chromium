// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_CLIENT_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_CLIENT_H_

#import <Foundation/Foundation.h>
#include <vector>

#include "base/compiler_specific.h"
#import "ios/web/public/web_client.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace web {

class BrowserState;

// A WebClient used for testing purposes.
class TestWebClient : public web::WebClient {
 public:
  TestWebClient();
  ~TestWebClient() override;

  // WebClient implementation.
  void AddAdditionalSchemes(Schemes* schemes) const override;

  // Returns true for kTestWebUIScheme and kTestNativeContentScheme URL schemes.
  bool IsAppSpecificURL(const GURL& url) const override;

  bool ShouldBlockUrlDuringRestore(const GURL& url,
                                   WebState* web_state) const override;

  void AddSerializableData(web::SerializableUserDataManager* user_data_manager,
                           web::WebState* web_state) override;

  std::string GetUserAgent(UserAgentType type) const override;

  // Returns |plugin_not_supported_text_| as the text to be displayed for an
  // unsupported plugin.
  base::string16 GetPluginNotSupportedText() const override;

  base::RefCountedMemory* GetDataResourceBytes(int id) const override;

  NSString* GetDocumentStartScriptForMainFrame(
      BrowserState* browser_state) const override;
  void AllowCertificateError(WebState*,
                             int cert_error,
                             const net::SSLInfo&,
                             const GURL&,
                             bool overridable,
                             int64_t navigation_id,
                             const base::Callback<void(bool)>&) override;
  void PrepareErrorPage(WebState* web_state,
                        const GURL& url,
                        NSError* error,
                        bool is_post,
                        bool is_off_the_record,
                        const base::Optional<net::SSLInfo>& info,
                        int64_t navigation_id,
                        base::OnceCallback<void(NSString*)> callback) override;
  UIView* GetWindowedContainer() override;

  // Sets |plugin_not_supported_text_|.
  void SetPluginNotSupportedText(const base::string16& text);

  // Changes Early Page Script for testing purposes.
  void SetEarlyPageScript(NSString* page_script);

  // Overrides AllowCertificateError response.
  void SetAllowCertificateErrors(bool flag);

  // Accessors for last arguments passed to AllowCertificateError.
  int last_cert_error_code() const { return last_cert_error_code_; }
  const net::SSLInfo& last_cert_error_ssl_info() const {
    return last_cert_error_ssl_info_;
  }
  const GURL& last_cert_error_request_url() const {
    return last_cert_error_request_url_;
  }
  bool last_cert_error_overridable() { return last_cert_error_overridable_; }

 private:
  base::string16 plugin_not_supported_text_;
  NSString* early_page_script_ = nil;
  // Last arguments passed to AllowCertificateError.
  int last_cert_error_code_ = 0;
  net::SSLInfo last_cert_error_ssl_info_;
  GURL last_cert_error_request_url_;
  bool last_cert_error_overridable_ = true;
  bool allow_certificate_errors_ = false;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_TEST_WEB_CLIENT_H_
