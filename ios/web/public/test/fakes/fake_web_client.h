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

  // Sets `find_session_prototype_` for testing purposes.
  void SetFindSessionPrototype(CRWFakeFindSession* find_session_prototype)
      API_AVAILABLE(ios(16));

  // Returns a copy of `find_session_prototype_` for testing purposes.
  id<CRWFindSession> CreateFindSessionForWebState(
      web::WebState* web_state) const override API_AVAILABLE(ios(16));

  // Sets `text_search_started_` to `true` for testing purposes.
  void StartTextSearchInWebState(web::WebState* web_state) override;

  // Sets `text_search_started_` to `false` for testing purposes.
  void StopTextSearchInWebState(web::WebState* web_state) override;

  // Returns `text_search_started_` for testing purposes.
  bool IsTextSearchStarted() const;

 private:
  std::u16string plugin_not_supported_text_;
  std::vector<JavaScriptFeature*> java_script_features_;
  NSString* early_page_script_for_all_frames_ = nil;
  NSString* early_page_script_for_main_frame_ = nil;
  UserAgentType default_user_agent_ = UserAgentType::MOBILE;
  CRWFakeFindSession* find_session_prototype_ API_AVAILABLE(ios(16)) = nil;
  bool text_search_started_ = false;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_WEB_CLIENT_H_
