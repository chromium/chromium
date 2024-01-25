// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_JAVA_SCRIPT_FEATURE_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_JAVA_SCRIPT_FEATURE_H_

#include <optional>

#import "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "ios/web/public/js_messaging/java_script_feature.h"
#include "url/gurl.h"

namespace web {
class WebState;
}  // namespace web

class SearchEngineJavaScriptFeatureDelegate {
 public:
  // Saves the page `url` generated from a <form> submission to create the
  // TemplateURL when the submission leads to a successful navigation.
  virtual void SetSearchableUrl(web::WebState* web_state, GURL url) = 0;

  // Adds a TemplateURL by downloading and parsing the OSDD.
  virtual void AddTemplateURLByOSDD(web::WebState* web_state,
                                    const GURL& page_url,
                                    const GURL& osdd_url) = 0;
};

// A feature which listens for search engine and website search box urls.
class SearchEngineJavaScriptFeature : public web::JavaScriptFeature {
 public:
  static SearchEngineJavaScriptFeature* GetInstance();

  // Sets `delegate` as the receiver for search engine url messages.
  void SetDelegate(SearchEngineJavaScriptFeatureDelegate* delegate);

 private:
  friend class base::NoDestructor<SearchEngineJavaScriptFeature>;

  SearchEngineJavaScriptFeature();
  ~SearchEngineJavaScriptFeature() override;

  SearchEngineJavaScriptFeature(const SearchEngineJavaScriptFeature&) = delete;
  SearchEngineJavaScriptFeature& operator=(
      const SearchEngineJavaScriptFeature&) = delete;

  // JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  raw_ptr<SearchEngineJavaScriptFeatureDelegate> delegate_;
};

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_JAVA_SCRIPT_FEATURE_H_
