// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_CLIENT_H_

#include <memory>

#include "base/compiler_specific.h"
#import "ios/web/public/web_client.h"

namespace ios_web_view {

// WebView implementation of WebClient.
class WebViewWebClient : public web::WebClient {
 public:
  WebViewWebClient();
  ~WebViewWebClient() override;

  // WebClient implementation.
  std::unique_ptr<web::WebMainParts> CreateWebMainParts() override;
  std::string GetProduct() const override;
  std::string GetUserAgent(web::UserAgentType type) const override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) const override;
  NSString* GetDocumentStartScriptForMainFrame(
      web::BrowserState* browser_state) const override;
  base::string16 GetPluginNotSupportedText() const override;
  void AllowCertificateError(
      web::WebState* web_state,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool overridable,
      const base::RepeatingCallback<void(bool)>& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewWebClient);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_CLIENT_H_
