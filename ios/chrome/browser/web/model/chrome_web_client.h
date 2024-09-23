// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHROME_WEB_CLIENT_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHROME_WEB_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#import "ios/web/public/web_client.h"

// Chrome implementation of WebClient.
class ChromeWebClient : public web::WebClient {
 public:
  ChromeWebClient();

  ChromeWebClient(const ChromeWebClient&) = delete;
  ChromeWebClient& operator=(const ChromeWebClient&) = delete;

  ~ChromeWebClient() override;

  // WebClient implementation.
  std::unique_ptr<web::WebMainParts> CreateWebMainParts() override;
  void PreWebViewCreation() const override;
  void AddAdditionalSchemes(Schemes* schemes) const override;
  std::string GetApplicationLocale() const override;
  bool IsAppSpecificURL(const GURL& url) const override;
  std::string GetUserAgent(web::UserAgentType type) const override;
  std::u16string GetLocalizedString(int message_id) const override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) const override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) const override;
  void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  void PostBrowserURLRewriterCreation(
      web::BrowserURLRewriter* rewriter) override;
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
  UIView* GetWindowedContainer() override;
  bool EnableFullscreenAPI() const override;
  bool EnableLongPressUIContextMenu() const override;
  bool EnableWebInspector(web::BrowserState* browser_state) const override;
  web::UserAgentType GetDefaultUserAgent(web::WebState* web_state,
                                         const GURL& url) const override;
  void LogDefaultUserAgent(web::WebState* web_state,
                           const GURL& url) const override;
  void CleanupNativeRestoreURLs(web::WebState* web_state) const override;
  void WillDisplayMediaCapturePermissionPrompt(
      web::WebState* web_state) const override;
  bool IsPointingToSameDocument(const GURL& url1,
                                const GURL& url2) const override;
  bool IsBrowserLockdownModeEnabled() override;
  void SetOSLockdownModeEnabled(bool enabled) override;
  bool IsInsecureFormWarningEnabled(
      web::BrowserState* browser_state) const override;

 private:
  // Reference to a view that is attached to a window.
  UIView* windowed_container_ = nil;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHROME_WEB_CLIENT_H_
