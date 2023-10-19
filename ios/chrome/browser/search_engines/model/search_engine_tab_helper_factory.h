// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_TAB_HELPER_FACTORY_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_TAB_HELPER_FACTORY_H_

#include "base/no_destructor.h"
#import "ios/chrome/browser/search_engines/model/search_engine_java_script_feature.h"

class SearchEngineTabHelperFactory
    : public SearchEngineJavaScriptFeatureDelegate {
 public:
  static SearchEngineTabHelperFactory* GetInstance();

 private:
  friend class base::NoDestructor<SearchEngineTabHelperFactory>;

  SearchEngineTabHelperFactory();
  ~SearchEngineTabHelperFactory();

  SearchEngineTabHelperFactory(const SearchEngineTabHelperFactory&) = delete;
  SearchEngineTabHelperFactory& operator=(const SearchEngineTabHelperFactory&) =
      delete;

  // SearchEngineJavaScriptFeatureDelegate:
  void SetSearchableUrl(web::WebState* web_state, GURL url) override;
  void AddTemplateURLByOSDD(web::WebState* web_state,
                            const GURL& page_url,
                            const GURL& osdd_url) override;
};

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINES_MODEL_SEARCH_ENGINE_TAB_HELPER_FACTORY_H_
