// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_CHROME_WEB_CLIENT_H_
#define IOS_CHROME_BROWSER_WEB_CHROME_WEB_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#import "ios/web/public/web_client.h"

// Shared desktop user agent used to mimic Safari on a mac.
extern const char kDesktopUserAgent[];

// Chrome implementation of WebClient.
class ChromeWebClient : public web::WebClient {
 public:
  ChromeWebClient();
  ~ChromeWebClient() override;

  // WebClient implementation.
  std::unique_ptr<web::WebMainParts> CreateWebMainParts() override;
  void PreWebViewCreation() const override;
  void AddAdditionalSchemes(Schemes* schemes) const override;
  std::string GetApplicationLocale() const override;
  bool IsAppSpecificURL(const GURL& url) const override;
  bool ShouldBlockUrlDuringRestore(const GURL& url,
                                   web::WebState* web_state) const override;
  void AddSerializableData(web::SerializableUserDataManager* user_data_manager,
                           web::WebState* web_state) override;
  base::string16 GetPluginNotSupportedText() const override;
  std::string GetUserAgent(web::UserAgentType type) const override;
  base::string16 GetLocalizedString(int message_id) const override;
  base::StringPiece GetDataResource(
      int resource_id,
      ui::ScaleFactor scale_factor) const override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) const override;
  void GetAdditionalWebUISchemes(
      std::vector<std::string>* additional_schemes) override;
  void PostBrowserURLRewriterCreation(
      web::BrowserURLRewriter* rewriter) override;
  NSString* GetDocumentStartScriptForAllFrames(
      web::BrowserState* browser_state) const override;
  NSString* GetDocumentStartScriptForMainFrame(
      web::BrowserState* browser_state) const override;
  void AllowCertificateError(
      web::WebState* web_state,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool overridable,
      int64_t navigation_id,
      const base::Callback<void(bool)>& callback) override;
  void PrepareErrorPage(web::WebState* web_state,
                        const GURL& url,
                        NSError* error,
                        bool is_post,
                        bool is_off_the_record,
                        const base::Optional<net::SSLInfo>& info,
                        int64_t navigation_id,
                        base::OnceCallback<void(NSString*)> callback) override;
  UIView* GetWindowedContainer() override;

 private:
  // Returns a string describing the product name and version, of the
  // form "productname/version". Used as part of the user agent string.
  std::string GetProduct() const;

  // Reference to a view that is attached to a window.
  UIView* windowed_container_ = nil;

  DISALLOW_COPY_AND_ASSIGN(ChromeWebClient);
};

#endif  // IOS_CHROME_BROWSER_WEB_CHROME_WEB_CLIENT_H_
