// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/search_engines/model/search_engine_tab_helper_factory.h"

#import "base/no_destructor.h"
#import "ios/chrome/browser/search_engines/model/search_engine_tab_helper.h"

// static
SearchEngineTabHelperFactory* SearchEngineTabHelperFactory::GetInstance() {
  static base::NoDestructor<SearchEngineTabHelperFactory> instance;
  return instance.get();
}

SearchEngineTabHelperFactory::SearchEngineTabHelperFactory() {}

SearchEngineTabHelperFactory::~SearchEngineTabHelperFactory() = default;

void SearchEngineTabHelperFactory::SetSearchableUrl(web::WebState* web_state,
                                                    GURL url) {
  SearchEngineTabHelper* search_engine_tab_helper =
      SearchEngineTabHelper::FromWebState(web_state);
  // SearchEngineTabHelper may not exist if WebState is being destroyed.
  if (!search_engine_tab_helper) {
    return;
  }
  search_engine_tab_helper->SetSearchableUrl(url);
}

void SearchEngineTabHelperFactory::AddTemplateURLByOSDD(
    web::WebState* web_state,
    const GURL& page_url,
    const GURL& osdd_url) {
  SearchEngineTabHelper* search_engine_tab_helper =
      SearchEngineTabHelper::FromWebState(web_state);
  // SearchEngineTabHelper may not exist if WebState is being destroyed.
  if (!search_engine_tab_helper) {
    return;
  }
  search_engine_tab_helper->AddTemplateURLByOSDD(page_url, osdd_url);
}
