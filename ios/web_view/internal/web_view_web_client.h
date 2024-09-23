// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_CLIENT_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_CLIENT_H_

#import <memory>
#import <optional>
#include <string_view>

#import "ios/web/public/web_client.h"

namespace ios_web_view {

// WebView implementation of WebClient.
class WebViewWebClient : public web::WebClient {
 public:
  WebViewWebClient();

  WebViewWebClient(const WebViewWebClient&) = delete;
  WebViewWebClient& operator=(const WebViewWebClient&) = delete;

  ~WebViewWebClient() override;

  // WebClient implementation.
  std::unique_ptr<web::WebMainParts> CreateWebMainParts() override;
  void AddAdditionalSchemes(Schemes* schemes) const override;
  bool IsAppSpecificURL(const GURL& url) const override;
  std::string GetUserAgent(web::UserAgentType type) const override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) const override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) const override;
  std::vector<web::JavaScriptFeature*> GetJavaScriptFeatures(
      web::BrowserState* browser_state) const override;
  void PrepareErrorPage(web::WebState* web_state,
                        const GURL& url,
                        NSError* error,
                        bool is_post,
                        bool is_off_the_record,
                        const std::optional<net::SSLInfo>& info,
                        int64_t navigation_id,
                        base::OnceCallback<void(NSString*)> callback) override;
  bool EnableLongPressUIContextMenu() const override;
  bool EnableWebInspector(web::BrowserState* browser_state) const override;
  bool IsInsecureFormWarningEnabled(
      web::BrowserState* browser_state) const override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_CLIENT_H_
