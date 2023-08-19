// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_CLIENT_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_CLIENT_H_

#import <Foundation/Foundation.h>
#include <vector>

#import "ios/web/public/web_client.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

@class CRWFakeFindSession;
@class UIFindSession;

namespace web {

class BrowserState;

// A WebClient used for testing purposes.
class FakeWebClient : public web::WebClient {
 public:
  FakeWebClient();
  ~FakeWebClient() override;

  // WebClient implementation.
  void AddAdditionalSchemes(Schemes* schemes) const override;

  // Returns true for kTestWebUIScheme URL.
  bool IsAppSpecificURL(const GURL& url) const override;

  std::string GetUserAgent(UserAgentType type) const override;

  base::RefCountedMemory* GetDataResourceBytes(int id) const override;

  std::vector<JavaScriptFeature*> GetJavaScriptFeatures(
      BrowserState* browser_state) const override;

  NSString* GetDocumentStartScriptForAllFrames(
      BrowserState* browser_state) const override;
  NSString* GetDocumentStartScriptForMainFrame(
      BrowserState* browser_state) const override;
  void PrepareErrorPage(WebState* web_state,
                        const GURL& url,
                        NSError* error,
                        bool is_post,
                        bool is_off_the_record,
                        const absl::optional<net::SSLInfo>& info,
                        int64_t navigation_id,
                        base::OnceCallback<void(NSString*)> callback) override;
  UIView* GetWindowedContainer() override;
  UserAgentType GetDefaultUserAgent(web::WebState* web_state,
                                    const GURL& url) const override;

  // Sets `plugin_not_supported_text_`.
  void SetPluginNotSupportedText(const std::u16string& text);

  // Changes Early Page Script for all frames for testing purposes.
  void SetEarlyPageScriptForAllFrames(NSString* page_script_for_all_frames);

  // Changes Early Page Script for the main frame for testing purposes.
  void SetEarlyPageScriptForMainFrame(NSString* page_script_for_main_frame);

  // Changes Java Script Features for testing.
  void SetJavaScriptFeatures(std::vector<JavaScriptFeature*> features);

  void SetDefaultUserAgent(UserAgentType type) { default_user_agent_ = type; }

 private:
  std::u16string plugin_not_supported_text_;
  std::vector<JavaScriptFeature*> java_script_features_;
  NSString* early_page_script_for_all_frames_ = nil;
  NSString* early_page_script_for_main_frame_ = nil;
  UserAgentType default_user_agent_ = UserAgentType::MOBILE;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_CLIENT_H_
